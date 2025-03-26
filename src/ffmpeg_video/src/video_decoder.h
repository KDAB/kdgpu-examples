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

using UploadCallback = std::function<void(AVFrame *, const std::array<size_t, 4> &)>;

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
    bool decodeFrame(UploadCallback uploadCallback);

    std::pair<size_t, size_t> videoResolution() const;

private:
    int decodePacket(AVPacket *packet, UploadCallback uploadCallback);

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
};
