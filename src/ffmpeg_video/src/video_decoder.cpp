/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group
  company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "video_decoder.h"

// Prevent C++ symbol mangling of the FFMpeg C library
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/stereo3d.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavdevice/avdevice.h>
}

#include <cstdint>
#include <thread>
#include <algorithm>
#include <array>
#include <chrono>

#include <spdlog/spdlog.h>

#include <KDUtils/elapsedtimer.h>

namespace {

const AVHWDeviceType targetHwType = AV_HWDEVICE_TYPE_CUDA;

} // namespace

VideoDecoder::VideoDecoder()
    : m_formatContext(avformat_alloc_context())
    , m_queue(std::make_unique<VideoQueue>())
{
    assert(m_formatContext != NULL);
}

VideoDecoder::~VideoDecoder()
{
    if (m_isOpen) {

        // Stop Decoding Thread
        if (m_decodingThreadCanRun.load()) {
            m_decodingThreadCanRun.store(false);
            m_queue->terminate();
            if (m_decodingThread.joinable())
                m_decodingThread.join();
            m_queue.reset();
        }

        avformat_close_input(&m_formatContext);
        m_isOpen = false;

        if (m_packet)
            av_packet_free(&m_packet);
        if (m_frame)
            av_frame_free(&m_frame);
        if (m_transferFrame)
            av_frame_free(&m_transferFrame);
        if (m_codecContext)
            avcodec_free_context(&m_codecContext);
    }

    avformat_free_context(m_formatContext);
}

bool VideoDecoder::open()
{
    if (avformat_open_input(&m_formatContext, mediaFile(), NULL, NULL) != 0) {
        SPDLOG_WARN("Failed to open video file: {}", mediaFile());
        return false;
    }
    m_isOpen = true;

    // Dump info in console
    av_dump_format(m_formatContext, 0, mediaFile(), 0);

    SPDLOG_WARN("format {}, duration {} us, bit_rate {}", m_formatContext->iformat->name, m_formatContext->duration, m_formatContext->bit_rate);
    return true;
}

bool VideoDecoder::selectCodecs()
{
    assert(isOpen());
    // Find best video decoder
    const AVCodec *pCodec = NULL;
    AVCodecParameters *pCodecParameters = NULL;

    for (int i = 0; i < m_formatContext->nb_streams; i++) {
        AVCodecParameters *localCodecParameters = m_formatContext->streams[i]->codecpar;
        SPDLOG_WARN("AVStream->time_base before open codec {}/{}", m_formatContext->streams[i]->time_base.num, m_formatContext->streams[i]->time_base.den);
        SPDLOG_WARN("AVStream->r_frame_rate before open codec {}/{}", m_formatContext->streams[i]->r_frame_rate.num, m_formatContext->streams[i]->r_frame_rate.den);
        SPDLOG_WARN("AVStream->start_time {}", m_formatContext->streams[i]->start_time);
        SPDLOG_WARN("AVStream->duration {}", m_formatContext->streams[i]->duration);

        const AVCodec *codec = avcodec_find_decoder(localCodecParameters->codec_id);
        if (codec == NULL)
            continue;

        SPDLOG_WARN("Codec {} ID {} bit_rate {}", codec->name, int(codec->id), localCodecParameters->bit_rate);

        // when the stream is a video we store its index, codec parameters and codec
        if (localCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (m_videoCodec == nullptr) {
                m_videoStreamIndex = i;
                m_videoCodec = codec;
                m_videoCodecParams = localCodecParameters;
            }
            SPDLOG_WARN("Video Codec: resolution {} x {}", localCodecParameters->width, localCodecParameters->height);
        } else if (localCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            SPDLOG_WARN("Audio Codec: {} channels, sample rate {}", localCodecParameters->ch_layout.nb_channels, localCodecParameters->sample_rate);
        }
    }

    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
    while ((hwType = av_hwdevice_iterate_types(hwType)) != targetHwType)
        ;
    if (hwType == targetHwType) {
        SPDLOG_WARN("CUDA Video Decoder Supported");
    }

    return m_videoCodec != nullptr;
}

std::pair<size_t, size_t> VideoDecoder::videoResolution() const
{
    assert(m_videoCodecParams != nullptr);
    return std::make_pair(m_videoCodecParams->width, m_videoCodecParams->height);
}

std::pair<size_t, size_t> VideoDecoder::timebase() const
{
    assert(m_formatContext != nullptr && m_videoStreamIndex >= 0);
    const auto timeBase = m_formatContext->streams[m_videoStreamIndex]->time_base;
    return std::make_pair(timeBase.num, timeBase.den);
}

VideoQueue *VideoDecoder::frameQueue()
{
    return m_queue.get();
}

AVPixelFormat hwPixFormat = AV_PIX_FMT_NONE;

static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p = pix_fmts;

    while (*p != AV_PIX_FMT_NONE) {
        if (*p == hwPixFormat)
            return *p;
        p++;
    }

    SPDLOG_WARN("Failed to get HW surface format.");
    return AV_PIX_FMT_NONE;
}

bool VideoDecoder::canDecode()
{
    assert(m_videoCodec);

    AVHWDeviceType hwType = targetHwType;
    m_supportsHWDecoding = false;
    constexpr bool useHWDecoding = true;
    if constexpr (useHWDecoding) {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(m_videoCodec, i);
            if (!config) {
                SPDLOG_WARN("Decoder {} does not support device type {}.", m_videoCodec->name, av_hwdevice_get_type_name(hwType));
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == hwType) {
                m_supportsHWDecoding = true;
                hwPixFormat = config->pix_fmt;
                SPDLOG_WARN("HW Pixel Format {}", int(hwPixFormat));
                break;
            }
        }
    }
    SPDLOG_WARN("Decoder HW support:{}", m_supportsHWDecoding);

    m_codecContext = avcodec_alloc_context3(m_videoCodec);
    if (!m_codecContext) {
        SPDLOG_WARN("failed to allocated memory for AVCodecContext");
        return false;
    }

    // Fill the codec context based on the values from the supplied codec parameters
    if (avcodec_parameters_to_context(m_codecContext, m_videoCodecParams) < 0) {
        SPDLOG_WARN("failed to copy codec params to codec context");
        return false;
    }

    const bool multiFrameMt = m_videoCodec->capabilities & AV_CODEC_CAP_FRAME_THREADS;
    const bool singleFrameMt = m_videoCodec->capabilities & AV_CODEC_CAP_SLICE_THREADS;
    SPDLOG_WARN("Codec supports Multithreading Multi Frame MT {} Slice Mt {}", multiFrameMt, singleFrameMt);
    if (multiFrameMt || singleFrameMt) {
        m_codecContext->thread_count = std::min(4u, std::thread::hardware_concurrency());
        m_codecContext->thread_type = (singleFrameMt) ? FF_THREAD_SLICE : FF_THREAD_FRAME;
    }

    auto hwDecoderInit = [](AVCodecContext *ctx, const enum AVHWDeviceType type) -> int {
        int err = 0;
        AVBufferRef *hwDeviceCtx = NULL;
        if ((err = av_hwdevice_ctx_create(&hwDeviceCtx, type, NULL, NULL, 0)) < 0) {
            SPDLOG_WARN("Failed to create specified HW device.");
            return err;
        }
        ctx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
        return err;
    };

    if (m_supportsHWDecoding) {
        // Set func ptr to select HW format based on target format set on hwPixFormat (see above);
        m_codecContext->get_format = &getHwFormat;

        if (hwDecoderInit(m_codecContext, hwType) < 0) {
            SPDLOG_WARN("Failed to initialize HW decoding");
            return false;
        }
    }

    SPDLOG_WARN("Video Frame Width {} Height {} Format {}", m_videoCodecParams->width, m_videoCodecParams->height, m_videoCodecParams->format);

    // Initialize the AVCodecContext to use the given AVCodec.
    if (avcodec_open2(m_codecContext, m_videoCodec, NULL) < 0) {
        SPDLOG_WARN("failed to open codec through avcodec_open2");
        return false;
    }

    m_frame = av_frame_alloc();
    if (!m_frame) {
        SPDLOG_WARN("failed to allocate memory for AVFrame");
        return false;
    }

    if (m_supportsHWDecoding) {
        m_transferFrame = av_frame_alloc();
        if (!m_transferFrame) {
            SPDLOG_WARN("failed to allocate memory for transfer AVFrame");
            return false;
        }
    }

    m_packet = av_packet_alloc();
    if (!m_packet) {
        SPDLOG_WARN("failed to allocate memory for AVPacket");
        return false;
    }

    return true;
}

void VideoDecoder::startDecoding()
{
    m_decodingThreadCanRun.store(true);
    m_decodingThread = std::thread(&VideoDecoder::decodeFrames, this);
}

void VideoDecoder::decodeFrames()
{
    while (m_decodingThreadCanRun.load()) {
        // fill the Packet with data from the Stream
        const int ret = av_read_frame(m_formatContext, m_packet);

        if (ret < 0) {
            // Error or EOF
            SPDLOG_WARN("Stopping Decoding Loop due to av_read_frame error");
            break;
        }

        // if it's the video stream
        if (m_packet->stream_index == m_videoStreamIndex) {
            const int response = decodePacket(m_packet);
            av_packet_unref(m_packet);
            if (response < 0) {
                SPDLOG_WARN("Stopping Decoding Loop due to packet decoding error");
                break;
            }
        }
    }
    m_decodingThreadCanRun.store(false);
}

int VideoDecoder::decodePacket(AVPacket *packet)
{
    KDUtils::ElapsedTimer timer;
    timer.start();
    const size_t elapsedBeforeSendPacket = timer.msecElapsed();

    // Supply raw packet data as input to a decoder
    int response = avcodec_send_packet(m_codecContext, packet);
    const size_t elapsedAfterSendPacket = timer.msecElapsed();

    if (response < 0) {
        SPDLOG_WARN("Error while sending a packet to the decoder: {}", response);
        return response;
    }

    while (response >= 0) {
        // Return decoded output data (into a frame) from a decoder
        const size_t elapsedBeforeRedeiveFrame = timer.msecElapsed();
        response = avcodec_receive_frame(m_codecContext, m_frame);
        const size_t elapsedAfterReceiveFrame = timer.msecElapsed();

        if (response == AVERROR(EAGAIN)) { // Nothing more to read for now
            return 0;
        }
        if (response == AVERROR_EOF) { // We've reached the end of the video
            return response;
        }

        if (response < 0) { // Something else when wrong
            SPDLOG_WARN("Error while receiving a frame from the decoder: {}", response);
            return response;
        }

        if (response >= 0) {
            SPDLOG_WARN(
                    "Frame {} (type={}, format={}) pts {} time_base {}/{}",
                    m_codecContext->frame_num,
                    av_get_picture_type_char(m_frame->pict_type),
                    m_frame->format,
                    m_frame->pts,
                    m_frame->time_base.num, m_frame->time_base.den);

            AVFrame *frameToUpload = m_frame;
            if (m_supportsHWDecoding && m_frame->format == hwPixFormat) {
                // Retrieve data from GPU to CPU
                if ((response = av_hwframe_transfer_data(m_transferFrame, m_frame, 0)) < 0) {
                    SPDLOG_WARN("Error transferring from GPU to system memory");
                    return response;
                }
                AVHWFramesContext *hwFramesCtx = (AVHWFramesContext *)m_frame->hw_frames_ctx->data;
                // Set Correct Format on Frame
                m_transferFrame->format = hwFramesCtx->sw_format;
                m_transferFrame->pts = m_frame->pts;
                frameToUpload = m_transferFrame;
            }

            const size_t elapsedAfterHWTransfer = timer.msecElapsed();

            std::array<size_t, 4> bufferSizes;
            std::array<ptrdiff_t, 4> lineSizes;
            lineSizes[0] = frameToUpload->linesize[0];
            lineSizes[1] = frameToUpload->linesize[1];
            lineSizes[2] = frameToUpload->linesize[2];
            lineSizes[3] = frameToUpload->linesize[3];
            av_image_fill_plane_sizes(bufferSizes.data(), (AVPixelFormat)frameToUpload->format, frameToUpload->height, lineSizes.data());

            // Deep Copy the Frame and put it in our queue (see av_frame_make_writable to see what needs copying)
            AVFrame *copy = av_frame_alloc();
            assert(copy != nullptr);
            copy->format = frameToUpload->format;
            copy->width = frameToUpload->width;
            copy->height = frameToUpload->height;
            copy->nb_samples = frameToUpload->nb_samples;
            copy->pts = frameToUpload->pts;
            bool wasCopiedSuccessfully = true;

            if (av_channel_layout_copy(&copy->ch_layout, &frameToUpload->ch_layout) < 0) {
                SPDLOG_WARN("Failed to copy channel layout");
                wasCopiedSuccessfully = false;
            }
            if (av_frame_get_buffer(copy, 0) < 0) {
                SPDLOG_WARN("Failed to copy channel layout");
                wasCopiedSuccessfully = false;
            }
            if (av_frame_copy(copy, frameToUpload) < 0) {
                SPDLOG_WARN("Failed to copy frame data");
                wasCopiedSuccessfully = false;
            }
            if (av_frame_copy_props(copy, frameToUpload) < 0) {
                SPDLOG_WARN("Failed to copy frame props");
                wasCopiedSuccessfully = false;
            }

            // Store Copy into queue
            if (wasCopiedSuccessfully)
                m_queue->addFrame(FrameData{ copy, bufferSizes });

            const size_t elapsedAfterCopyAndStoreTransfer = timer.msecElapsed();

            SPDLOG_WARN("Decoding: Send Packet {}ms, Receive Frame {}ms HW Transfer: {}ms VideoQueue Store: {}ms",
                        elapsedAfterSendPacket - elapsedBeforeSendPacket,
                        elapsedAfterReceiveFrame - elapsedBeforeRedeiveFrame,
                        elapsedAfterHWTransfer - elapsedAfterReceiveFrame,
                        elapsedAfterCopyAndStoreTransfer - elapsedAfterHWTransfer);
        }
    }
    return response;
}

VideoQueue::VideoQueue() = default;

VideoQueue::~VideoQueue()
{
    // Thread ought to have been stopped by this point
    for (AVFrame *f : m_framesToRelease)
        av_frame_free(&f);
    m_framesToRelease.clear();

    for (FrameData &f : m_frames) {
        if (f.frame) {
            av_frame_free(&f.frame);
        }
    }
}

void VideoQueue::addFrame(const FrameData &frame)
{
    // Wait until we have enough room to add new frame
    while (true) {
        if (m_terminate.load())
            return;
        if (frameCount() < MaxFrameCount)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::scoped_lock lock{ m_mutex };
    ++m_frameCount;

    // Store new frames
    m_frames[m_frameCount - 1] = frame;
}

std::optional<FrameData> VideoQueue::lastFrame()
{
    std::scoped_lock lock{ m_mutex };
    // Check if we have a frame to return
    if (m_frameCount == 0)
        return {};
    return m_frames[m_frameCount - 1];
}

std::optional<FrameData> VideoQueue::takeFrame()
{
    std::scoped_lock lock{ m_mutex };

    // Release Frames Marked for Release
    for (AVFrame *f : m_framesToRelease)
        av_frame_free(&f);
    m_framesToRelease.clear();

    // Check if we have a frame to return
    if (m_frameCount == 0)
        return {};

    // Decr Frame Count and retrieve Frame Data

    --m_frameCount;
    FrameData out = m_frames[m_frameCount];
    // Store frame for release on the next call to takeFrame (as we know by then it will have been copied or discarded)
    m_framesToRelease.push_back(out.frame);
    return out;
}

size_t VideoQueue::frameCount()
{
    std::scoped_lock lock{ m_mutex };
    return m_frameCount;
}

void VideoQueue::terminate()
{
    return m_terminate.store(true);
}
