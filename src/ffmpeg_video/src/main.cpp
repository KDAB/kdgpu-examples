
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
    size_t lastAcquiredFramePtsMsecs = 0;
    size_t msecsAtLastFrameUpload = 0;

    bool videoInfoSet = false;
    const auto videoTimeBase = decoder.timebase();
    const float timeBaseF = float(videoTimeBase.first) / float(videoTimeBase.second); // eg (1 / 2400) for 24 fps
    const float timeBaseSecs = timeBaseF * 100.0f; // eg (1 / 2400) * 100 -> 1/24 of a sec
    const float timeBaseMsecs = timeBaseSecs * 1000.0f; // ~41ms
    view.onUpdate.connect([&](VideoRenderer *view) {
                     // Decode Video Frame and Upload to GPU Texture
                     const size_t elapsedBeforeAcquire = timer.msecElapsed();
                     const size_t dt = elapsedBeforeAcquire - elapsedAtLastFrame;
                     elapsedAtLastFrame = elapsedBeforeAcquire;

                     size_t elapsedAfterFrameAcquire = elapsedBeforeAcquire;
                     size_t elapsedAfterUpload = elapsedBeforeAcquire;

                     // Check if we have a new frame to upload from the VideoQueue
                     std::optional<FrameData> frameData = decoder.frameQueue()->lastFrame();
                     if (!frameData)
                         return;

                     AVFrame *frame = frameData->frame;
                     const auto &bufferSizes = frameData->bufferSizes;

                     const float pts = timeBaseF * frame->pts; // secs
                     const float ptsMsecs = pts * 1000.0f;

                     if (videoInfoSet) {
                         // Check if the timestamp of the frame requires us to upload it
                         const float deltaPtsSinceLastFrame = ptsMsecs - lastAcquiredFramePtsMsecs;

                         // Dealing with outdated frame (because we can receive frames out of order)
                         if (deltaPtsSinceLastFrame < 0) {
                             // Got some outdated frame, skipping and release it
                             SPDLOG_WARN("Outdated frame");

                             // Release Entry in the Queue
                             decoder.frameQueue()->takeFrame();
                             return;
                         }

                         const size_t nowMsecs = timer.msecElapsed();
                         const float delta = nowMsecs - msecsAtLastFrameUpload; // Between now and last upload
                         const float mscecWhenToUploadNextFrame = timeBaseMsecs - dt;

                         if (delta < mscecWhenToUploadNextFrame) {
                             // SPDLOG_WARN("Too early to present next frame, duplicating existing frame, can use next frame in {}ms", (mscecWhenToUploadNextFrame - delta));
                             return;
                         }
                     }
                     elapsedAfterFrameAcquire = timer.msecElapsed();

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
                     msecsAtLastFrameUpload = timer.msecElapsed();
                     elapsedAfterUpload = msecsAtLastFrameUpload;
                     lastAcquiredFramePtsMsecs = ptsMsecs;

                     const size_t acquireTime = elapsedAfterFrameAcquire - elapsedBeforeAcquire;
                     const size_t uploadTime = elapsedAfterUpload - elapsedAfterFrameAcquire;
                     SPDLOG_WARN("OnUpdate dt: {}ms acquire: {}ms, upload: {}ms, timer before: {}ms, after: {}ms", dt, acquireTime, uploadTime, elapsedBeforeAcquire, elapsedAfterUpload);

                     // Release Entry in the Queue
                     decoder.frameQueue()->takeFrame();
                 })
            .release();

    // Launch Decoding Thread
    decoder.startDecoding();

    return app.exec();
}
