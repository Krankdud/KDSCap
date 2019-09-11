#pragma once
#include <cstdio>
#include <SDL.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
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
    AVFrame* frame, *tempFrame;
    AVPacket* packet;

    SwrContext* swrContext;

    FILE* output;

    AVFrame* createFrame(int samples, int format, uint64_t channels);
    void initSwrContext();

    void encode(AVFrame* frame);
    void printError(int code);
};
