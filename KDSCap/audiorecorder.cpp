#include <memory.h>
#include <stdexcept>
#include "audiorecorder.h"

AudioRecorder::AudioRecorder()
{
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec)
    {
        throw std::runtime_error("Codec not found");
    }

    context = avcodec_alloc_context3(codec);
    if (!context)
    {
        throw std::runtime_error("Could not allocate audio codec context");
    }
    context->bit_rate = 128000;
    context->sample_fmt = AV_SAMPLE_FMT_S16;
    context->sample_rate = 44100;
    context->channel_layout = AV_CH_LAYOUT_STEREO;
    context->channels = 2;

    if (avcodec_open2(context, codec, NULL) < 0)
    {
        throw std::runtime_error("Could not open codec");
    }

    auto error = fopen_s(&output, "audio.mp2", "wb");
    if (error != 0)
    {
        throw std::runtime_error("Could not open file");
    }

    packet = av_packet_alloc();
    if (!packet)
    {
        throw std::runtime_error("Could not allocate audio packet");
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        throw std::runtime_error("Could not allocate audio frame");
    }

    frame->nb_samples = context->frame_size;
    frame->format = context->sample_fmt;
    frame->channel_layout = context->channel_layout;

    if (av_frame_get_buffer(frame, 0) < 0)
    {
        throw std::runtime_error("Could not allocate audio data buffers");
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = context->sample_rate;
    want.format = AUDIO_S16;
    want.channels = context->channels;
    want.samples = context->frame_size;
    want.callback = [](void* userdata, uint8_t* stream, int len) -> void {
        (( AudioRecorder*) userdata)->recordingCallback(stream, len);
    };
    want.userdata = this;
    
    int numDevices = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < numDevices; ++i)
    {
        printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 1));
    }
    const char* deviceName = SDL_GetAudioDeviceName(2, 1);

    device = SDL_OpenAudioDevice(deviceName, 1, &want, &recordingFormat, 0);
}

AudioRecorder::~AudioRecorder()
{
    if (device != 0)
    {
        SDL_PauseAudioDevice(device, 1);
        SDL_CloseAudioDevice(device);
    }

    fclose(output);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
}

void AudioRecorder::start()
{
    SDL_PauseAudioDevice(device, 0);
}

void AudioRecorder::stop()
{
    SDL_PauseAudioDevice(device, 1);
}

void AudioRecorder::recordingCallback(uint8_t* stream, int len)
{
    if (av_frame_make_writable(frame) < 0)
    {
        throw std::runtime_error("Could not ensure that the audio frame is writable");
    }

    avcodec_fill_audio_frame(frame, 2, AV_SAMPLE_FMT_S16, stream, len, 0);

    encode();
}

void AudioRecorder::encode()
{
    int ret;
    ret = avcodec_send_frame(context, frame);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
            throw std::runtime_error("Error encoding audio frame");
        
        fwrite(packet->data, 1, packet->size, output);
        av_packet_unref(packet);
    }
}
