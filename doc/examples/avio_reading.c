/*
 * Copyright (c) 2014 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat AVIOContext API example.
 *
 * Make libavformat demuxer access media content through a custom
 * AVIOContext read callback.
 * @example avio_reading.c
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    //printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

int main(int argc, char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };

    uint8_t au8_tmp[128];
    const char *video_codec_name, *audio_codec_name;
    FILE *pf_video = NULL, *pf_audio = NULL;
    int video_frames = 0, audio_frames = 0;
    int i = 0, video_stream_index = -1, audio_stream_index = -1;

    AVRational video_time_base = {0}, audio_time_base = {0};

    int64_t last_video_pts = 0, last_audio_pts = 0;
    int64_t last_video_dts = 0, last_audio_dts = 0;

    // BSF
    int bMp4H264 = 0;
    AVBitStreamFilterContext *bsfc = NULL;

    AVPacket pkt;
    AVPacket pktFiltered;
    av_init_packet(&pkt);
    av_init_packet(&pktFiltered);
    pkt.data = NULL;
    pkt.size = 0;
    pktFiltered.data = NULL;
    pktFiltered.size = 0;
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file\n"
                "API example program to show how to read from a custom buffer "
                "accessed through AVIOContext.\n", argv[0]);
        return 1;
    }
    input_filename = argv[1];

    /* register codecs and formats and other lavf/lavc components*/
    av_register_all();

    /* slurp file content into buffer */
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0)
        goto end;

    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = buffer;
    bd.size = buffer_size;

    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }

    av_dump_format(fmt_ctx, 0, input_filename, 0);

    /*
    for (i = 0; i < fmt_ctx->nb_programs; i++) {
        AVProgram *prog = fmt_ctx->programs[i];
        fprintf(stderr, "id=%d nb_stream_indexes=%u program_num=%d\n", prog->id, prog->nb_stream_indexes, prog->program_num);
    }
    */

    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];
        if ((video_stream_index == -1) && (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)) {
            video_stream_index = i;
            video_codec_name   = avcodec_get_name(stream->codec->codec_id);
            video_time_base    = stream->time_base;
            fprintf(stderr, "video: index=%d codec_type=%s\n", i, video_codec_name);

            snprintf(au8_tmp, 127, "out.%s", video_codec_name);
            pf_video = fopen(au8_tmp, "wb");
        } else if ((audio_stream_index == -1) && (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)) {
            audio_stream_index = i;
            audio_codec_name   = avcodec_get_name(stream->codec->codec_id);
            audio_time_base    = stream->time_base;
            fprintf(stderr, "audio: index=%d codec_type=%s\n", i, audio_codec_name);

            snprintf(au8_tmp, 127, "out.%s", audio_codec_name);
            pf_audio = fopen(au8_tmp, "wb");
        }
    }

    if ((video_stream_index != -1) && (audio_stream_index != -1)) {
        fprintf(stderr, "find video and audio stream ok.\n");
    }

    bMp4H264 = !strcmp(video_codec_name, "h264") &&
            (!strcmp(fmt_ctx->iformat->long_name, "QuickTime / MOV")    ||
             !strcmp(fmt_ctx->iformat->long_name, "FLV (Flash Video)") ||
             !strcmp(fmt_ctx->iformat->long_name, "Matroska / WebM"));
    if (bMp4H264) {
        bsfc = av_bitstream_filter_init("h264_mp4toannexb");
        if (!bsfc) {
            fprintf(stderr, "av_bitstream_filter_init failed.");
            goto end;
        }
    }
    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        //AVPacket orig_pkt = pkt;
        if (pkt.stream_index == video_stream_index) {
            if (bMp4H264) {
                AVPacket new_pkt = pkt;
                int a = av_bitstream_filter_filter(bsfc, fmt_ctx->streams[video_stream_index]->codec, NULL, 
                        &new_pkt.data, &new_pkt.size,
                        pkt.data, pkt.size, pkt.flags & AV_PKT_FLAG_KEY);
                if (a == 0 && new_pkt.data != pkt.data) {
                    uint8_t *t = av_malloc(new_pkt.size + AV_INPUT_BUFFER_PADDING_SIZE);
                    if (t) {
                        memcpy(t, new_pkt.data, new_pkt.size);
                        memset(t + new_pkt.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
                        new_pkt.data = t;
                        new_pkt.buf  = NULL;
                        a = 1;
                    } else {
                        fprintf(stderr, "av_malloc failed.");
                        goto end;
                    }
                }
                if (a > 0) {
                    pkt.side_data = NULL;
                    pkt.side_data_elems = 0;
                    av_free_packet(&pkt);
                    new_pkt.buf = av_buffer_create(new_pkt.data, new_pkt.size, av_buffer_default_free, NULL, 0);
                    if (!new_pkt.buf) {
                        fprintf(stderr, "av_buffer_create failed.");
                        goto end;
                    }
                } else if (a < 0) {
                    fprintf(stderr, "av_bitstream_filter_filter failed.");
                    goto end;
                }
                pkt = new_pkt;
            }

            fwrite(pkt.data, 1, pkt.size, pf_video);
            video_frames++;
            last_video_pts = pkt.pts * 1000 * video_time_base.num / video_time_base.den;
            last_video_dts = pkt.dts * 1000 * video_time_base.num / video_time_base.den;
            fprintf(stderr, "video packet %d size %d pts %I64d dts %I64d\n", video_frames, pkt.size, last_video_pts, last_video_dts);
        }

        if (pkt.stream_index == audio_stream_index) {
            fwrite(pkt.data, 1, pkt.size, pf_audio);
            audio_frames++;
            last_audio_pts = pkt.pts * 1000 * audio_time_base.num / audio_time_base.den;
            last_audio_dts = pkt.dts * 1000 * audio_time_base.num / audio_time_base.den;
            fprintf(stderr, "audio packet %d size %d pts %I64d dts %I64d\n", audio_frames, pkt.size, last_audio_pts, last_audio_dts);
        }

        av_free_packet(&pkt);
    }
    fprintf(stderr, "video frames: %d last video pts: %I64d audio frames: %d last audio pts: %I64d\n", video_frames, last_video_pts, audio_frames, last_audio_pts);

end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    av_file_unmap(buffer, buffer_size);

    if (bsfc) {
        av_bitstream_filter_close(bsfc);
    }

    if (pf_video) {
        fclose(pf_video);
        pf_video = NULL;
    }
    
    if (pf_audio) {
        fclose(pf_audio);
        pf_audio = NULL;
    }

    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}
