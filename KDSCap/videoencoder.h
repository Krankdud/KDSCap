#pragma once
#include <cstdio>
extern "C" {
    #include <libavcodec/avcodec.h>
}

class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    void sendFrame(uint16_t* buffer);

private:
    const AVCodec* codec;
    AVCodecContext* context;
    AVFrame* frame;
    AVPacket* packet;
    
    FILE* output;

    int frameCount = 0;

    void encode(AVFrame* frame);
};

