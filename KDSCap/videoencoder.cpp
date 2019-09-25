#include <stdexcept>
#include <SDL_surface.h>
#include "screenmodes.h"
#include "videoencoder.h"

extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

VideoEncoder::VideoEncoder()
{
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        throw std::runtime_error("Could not find video codec");
    }

    context = avcodec_alloc_context3(codec);
    if (!context)
    {
        throw std::runtime_error("Could not allocate video codec context");
    }

    context->width = DS_WIDTH;
    context->height = DS_HEIGHT;
    context->pix_fmt = AV_PIX_FMT_YUV444P;
    context->time_base = {1, 60};
    context->framerate = {60, 1};
    //context->bit_rate = 400000;
    //context->gop_size = 10;
    //context->max_b_frames = 1;
    av_opt_set(context->priv_data, "preset", "ultrafast", 0);

    if (avcodec_open2(context, codec, NULL) < 0)
    {
        throw std::runtime_error("Could not open context");
    }

    auto error = fopen_s(&output, "video.mp4", "wb");
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
        throw std::runtime_error("Could not allocate video frame");
    }
    frame->format = context->pix_fmt;
    frame->width = context->width;
    frame->height = context->height;

    if (av_frame_get_buffer(frame, 0) < 0)
    {
        throw std::runtime_error("Could not allocate the video frame data");
    }
}

VideoEncoder::~VideoEncoder()
{
    uint8_t endcode[] = {0, 0, 1, 0xb7};

    encode(NULL); // flush the encoder
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    fwrite(endcode, 1, sizeof(endcode), output); // Add sequence end code
    fclose(output);
}

void VideoEncoder::sendFrame(uint16_t* buffer)
{
    uint16_t dstBuffer[DS_WIDTH + DS_HEIGHT * 2];
    av_frame_make_writable(frame);

    SDL_ConvertPixels(DS_WIDTH, DS_HEIGHT, SDL_PIXELFORMAT_RGB565, buffer, 0, SDL_PIXELFORMAT_IYUV, dstBuffer, 0);

    if (av_frame_make_writable(frame) < 0)
    {
        throw std::runtime_error("Could not make frame writable");
    }

    if (av_image_fill_arrays(frame->data, frame->linesize, NULL, AV_PIX_FMT_YUV444P, DS_WIDTH, DS_HEIGHT * 2, DS_WIDTH) < 0)
    {
        throw std::runtime_error("Could not fill image");
    }

    encode(frame);
}

void VideoEncoder::encode(AVFrame* frame)
{
    int ret;
    ret = avcodec_send_frame(context, frame);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
            throw std::runtime_error("Error encoding video frame");
        
        fwrite(packet->data, 1, packet->size, output);
        av_packet_unref(packet);
    }
}
