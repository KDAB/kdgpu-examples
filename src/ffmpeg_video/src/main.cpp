
/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group
  company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "video_renderer.h"
#include "video_decoder.h"

#include <KDGui/gui_application.h>
#include <KDUtils/elapsedtimer.h>

extern "C" {
#include <libavutil/frame.h>
}

int main()
{
    KDGui::GuiApplication app;
    app.applicationName = "Stereo-Video";

    VideoDecoder decoder;
    decoder.mediaFile = "videos/big_buck_bunny_720p_h264.mov";

    // Open Video File
    if (!decoder.open())
        return -1;

    // Can we decode it?
    if (!decoder.selectCodecs())
        return -1;

    if (!decoder.canDecode())
        return -1;

    VideoRenderer view; // Create Window and Start Render Loop

    KDUtils::ElapsedTimer timer;
    timer.start();
    size_t elapsedAtLastFrame = timer.msecElapsed();

    bool videoInfoSet = false;
    view.onUpdate.connect([&](VideoRenderer *view) {
                     // Decode Video Frame and Upload to GPU Texture
                     const size_t elapsedBeforeDecoding = timer.msecElapsed();
                     const size_t dt = elapsedBeforeDecoding - elapsedAtLastFrame;
                     elapsedAtLastFrame = elapsedBeforeDecoding;

                     size_t elapsedAfterDecoding = elapsedBeforeDecoding;
                     size_t elapsedAfterUpload = elapsedBeforeDecoding;
                     const bool didDecode = decoder.decodeFrame([&](AVFrame *frame, const std::array<size_t, 4> &bufferSizes) {
                         elapsedAfterDecoding = timer.msecElapsed();

                         if (!videoInfoSet) {
                             videoInfoSet = true;
                             assert(frame->format == AVPixelFormat::AV_PIX_FMT_YUV420P // Software Decoding Format
                                    || frame->format == AVPixelFormat::AV_PIX_FMT_NV12); // CUDA HW Decoding Format
                             const KDGpu::Format format = (frame->format == AV_PIX_FMT_NV12) ? KDGpu::Format::G8_B8R8_2PLANE_420_UNORM : KDGpu::Format::G8_B8_R8_3PLANE_420_UNORM;
                             view->setVideoInformation({ frame->width, frame->height }, format);
                             SPDLOG_WARN("Width: {}, Height: {}, Format: {}", frame->width, frame->height, frame->format);
                             SPDLOG_WARN("Line Sizes [0]:{}, [1]:{}, [2]:{}, [3]:{}", frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3]);
                             SPDLOG_WARN("Data[0]: {:p}, Data[1]: {:p}, Data[2]: {:p}, Data[3]: {:p}", (void *)frame->data[0], (void *)frame->data[1], (void *)frame->data[2], (void *)frame->data[3]);
                             SPDLOG_WARN("BufferSize[0]: {}, BufferSize[1]: {}, BufferSize[2]: {}, BufferSize[3]: {}", bufferSizes[0], bufferSizes[1], bufferSizes[2], bufferSizes[3]);
                         }

                         view->uploadVideoFrameData(frame, bufferSizes);
                         elapsedAfterUpload = timer.msecElapsed();
                     });
                     if (didDecode) {
                         const size_t decodingTime = elapsedAfterDecoding - elapsedBeforeDecoding;
                         const size_t uploadTime = elapsedAfterUpload - elapsedAfterDecoding;
                         SPDLOG_WARN("OnUpdate dt: {}ms decoding: {}ms, upload: {}ms, timer before: {}ms, after: {}ms", dt, decodingTime, uploadTime, elapsedBeforeDecoding, elapsedAfterDecoding);
                     }
                 })
            .release();

    return app.exec();
}
