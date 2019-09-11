#include <memory.h>
#include <stdexcept>
#include "audiorecorder.h"

AudioRecorder::AudioRecorder()
{
    codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
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
    context->sample_fmt = AV_SAMPLE_FMT_FLTP;
    context->sample_rate = 44100;
    context->channel_layout = AV_CH_LAYOUT_STEREO;
    context->channels = av_get_channel_layout_nb_channels(context->channel_layout);

    int av_error = avcodec_open2(context, codec, NULL);
    if (av_error < 0)
    {
        printError(av_error);
        throw std::runtime_error("Could not open codec");
    }

    auto error = fopen_s(&output, "audio.mp3", "wb");
    if (error != 0)
    {
        throw std::runtime_error("Could not open file");
    }

    packet = av_packet_alloc();
    if (!packet)
    {
        throw std::runtime_error("Could not allocate audio packet");
    }

    frame = createFrame(context->frame_size, context->sample_fmt, context->channel_layout);
    // Create a temporary frame for converting from SDL's floating point audio
    // to floating point planar for the MP3 codec.
    tempFrame = createFrame(context->frame_size, AV_SAMPLE_FMT_FLT, context->channel_layout);

    initSwrContext();

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = context->sample_rate;
    want.format = AUDIO_F32;
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

    encode(NULL); // flush the encoder
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    swr_free(&swrContext);
    fclose(output);
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
    if (av_frame_make_writable(tempFrame) < 0)
    {
        throw std::runtime_error("Could not ensure that the audio frame is writable");
    }

    int av_error = avcodec_fill_audio_frame(tempFrame, 2, AV_SAMPLE_FMT_FLT, stream, len, 0);
    if (av_error < 0)
    {
        printError(av_error);
    }

    av_error = swr_convert_frame(swrContext, frame, tempFrame);
    if (av_error < 0)
    {
        printError(av_error);
    }

    encode(frame);
}

void AudioRecorder::encode(AVFrame* frame)
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

void AudioRecorder::printError(int code)
{
    char buffer[1024];
    av_make_error_string(buffer, 1024, code);
    printf(buffer);
    printf("\n");
}

void AudioRecorder::initSwrContext()
{
    swrContext = swr_alloc_set_opts(swrContext,
                                    context->channel_layout,
                                    context->sample_fmt,
                                    context->sample_rate,
                                    context->channel_layout,
                                    AV_SAMPLE_FMT_FLT,
                                    context->sample_rate,
                                    0,
                                    NULL);
    swr_init(swrContext);
}

AVFrame* AudioRecorder::createFrame(int samples, int format, uint64_t channels)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        throw std::runtime_error("Could not allocate audio frame");
    }

    frame->nb_samples = samples;
    frame->format = format;
    frame->channel_layout = channels;
    frame->sample_rate = context->sample_rate;

    int av_error = av_frame_get_buffer(frame, 0);
    if (av_error < 0)
    {
        printError(av_error);
        throw std::runtime_error("Could not allocate audio data buffers");
    }

    return frame;
}
