#include <obs-module.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

struct mem_block {
    uint8_t *data;
    size_t size;
    size_t num_read;
};

// Define the read_packet function for avio_alloc_context
int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct mem_block *img_block = (struct mem_block *)opaque;
    int remaining_size = img_block->size - img_block->num_read;
    int size = buf_size < remaining_size ? buf_size : remaining_size;

    if (size > 0) {
        memcpy(buf, img_block->data + img_block->num_read, size);
        img_block->num_read += size;
    }

    return size;
}

bool load_image_from_memory(uint8_t *img_data, uint32_t data_size, uint8_t *dest) {
    struct mem_block img_block;
    img_block.data = img_data;
    img_block.num_read = 0;
    img_block.size = data_size;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVFrame *frame = NULL, *frameRGB = NULL;
    AVPacket *packet = NULL;
    struct SwsContext *sws_ctx = NULL;
    int ret;

    avformat_network_init();

    // Initialize format context with memory
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        fprintf(stderr, "Could not allocate format context\n");
        return false;
    }

    AVIOContext *avio_ctx = NULL;
    unsigned char *avio_ctx_buffer = NULL;
    size_t avio_ctx_buffer_size = 4096;

    avio_ctx_buffer = (unsigned char *)av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        fprintf(stderr, "Could not allocate AVIO context buffer\n");
        return false;
    }

    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, (void *)&img_block, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        fprintf(stderr, "Could not allocate AVIO context\n");
        return false;
    }

    fmt_ctx->pb = avio_ctx;

    // Open input from memory
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        return false;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return false;
    }

    AVCodecParameters *codecpar = fmt_ctx->streams[0]->codecpar;
    codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Could not find codec\n");
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
        return false;
    }

    ret = avcodec_parameters_to_context(codec_ctx, codecpar);
    if (ret < 0) {
        fprintf(stderr, "Could not copy codec context\n");
        return false;
    }

    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

    frame = av_frame_alloc();
    frameRGB = av_frame_alloc();
    if (!frame || !frameRGB) {
        fprintf(stderr, "Could not allocate frame\n");
        return false;
    }

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, codec_ctx->width, codec_ctx->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGBA, codec_ctx->width, codec_ctx->height, 1);

    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                             codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGBA,
                             SWS_BILINEAR, NULL, NULL, NULL);

    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate packet\n");
        return false;
    }

    while (av_read_frame(fmt_ctx, packet) >= 0) {
        ret = avcodec_send_packet(codec_ctx, packet);
        if (ret < 0) {
            fprintf(stderr, "Error sending packet for decoding\n");
            return false;
        }

        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return false;
        }

        // Convert the image from its native format to RGBA
        sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize,
                  0, codec_ctx->height, frameRGB->data, frameRGB->linesize);

        // Copy the RGBA data to dest
        uint8_t *dest_ptr = dest;
        for (int row = 0; row < codec_ctx->height; ++row) {
            for (int col = 0; col < codec_ctx->width; ++col) {
                int rgba_index = row * frameRGB->linesize[0] + col * 4;
                *dest_ptr++ = frameRGB->data[0][rgba_index + 2]; // B
                *dest_ptr++ = frameRGB->data[0][rgba_index + 1]; // G
                *dest_ptr++ = frameRGB->data[0][rgba_index + 0]; // R
                *dest_ptr++ = frameRGB->data[0][rgba_index + 3]; // A
            }
        }

        av_packet_unref(packet);
        break; // We only need the first frame for a single image
    }

    av_free(buffer);
    av_frame_free(&frameRGB);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    avformat_free_context(fmt_ctx);
    av_free(avio_ctx->buffer);
    av_free(avio_ctx);

    return true;
}
