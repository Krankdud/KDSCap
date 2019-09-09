#pragma once
#include <cstdio>
#include <SDL.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

class AudioRecorder
{
public:
    AudioRecorder();
    ~AudioRecorder();

    void start();
    void stop();
    
    void recordingCallback(uint8_t* stream, int len);

private:
    SDL_AudioDeviceID device;
    SDL_AudioSpec recordingFormat;

    const AVCodec* codec;
    AVCodecContext* context;
    AVFrame* frame;
    AVPacket* packet;

    FILE* output;

    void encode();
};
