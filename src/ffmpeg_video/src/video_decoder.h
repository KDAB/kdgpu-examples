/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group
  company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

// Prevent C++ symbol mangling of the FFMpeg C library
extern "C" {
struct AVCodec;
struct AVCodecContext;
struct AVCodecParameters;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
}

#include <kdbindings/property.h>
#include <functional>
#include <array>
#include <mutex>
#include <thread>

using UploadCallback = std::function<void(AVFrame *, const std::array<size_t, 4> &)>;

struct FrameData {
    AVFrame *frame{ nullptr };
    std::array<size_t, 4> bufferSizes{ 0, 0, 0, 0 };
};

constexpr size_t MaxFrameCount = 5;
class VideoQueue
{
public:
    VideoQueue();
    ~VideoQueue();
    void addFrame(const FrameData &frame);
    std::optional<FrameData> lastFrame();
    std::optional<FrameData> takeFrame();
    size_t frameCount();
    void terminate();

private:
    std::array<FrameData, MaxFrameCount> m_frames;
    size_t m_frameCount = 0;
    std::mutex m_mutex;
    std::atomic<bool> m_terminate{ false };

    std::vector<AVFrame *> m_framesToRelease;
};

class VideoDecoder
{
public:
    KDBindings::Property<const char *> mediaFile{};

    VideoDecoder();
    ~VideoDecoder();

    bool open();
    bool isOpen() const { return m_isOpen; }

    bool selectCodecs();
    bool canDecode();
    void startDecoding();

    std::pair<size_t, size_t> videoResolution() const;
    std::pair<size_t, size_t> timebase() const;
    VideoQueue *frameQueue();

private:
    void decodeFrames();
    int decodePacket(AVPacket *packet);

    AVFormatContext *m_formatContext{ nullptr };
    bool m_isOpen{ false };
    const AVCodec *m_videoCodec{ nullptr };
    AVCodecParameters *m_videoCodecParams{ nullptr };
    int m_videoStreamIndex{ -1 };

    AVCodecContext *m_codecContext{ nullptr };
    AVFrame *m_frame{ nullptr };
    AVFrame *m_transferFrame{ nullptr };
    AVPacket *m_packet{ nullptr };
    bool m_supportsHWDecoding{ false };

    std::thread m_decodingThread;
    std::atomic<bool> m_decodingThreadCanRun{ true };

    std::unique_ptr<VideoQueue> m_queue;
};
