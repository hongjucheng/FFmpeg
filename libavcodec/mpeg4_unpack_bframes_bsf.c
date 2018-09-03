/*
 * Bitstream filter for unpacking DivX-style packed B-frames in MPEG-4 (divx_packed)
 * Copyright (c) 2015 Andreas Cadhalpun <Andreas.Cadhalpun@googlemail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "bsf.h"
#include "mpeg4video.h"

typedef struct UnpackBFramesBSFContext {
    AVPacket *b_frame;
} UnpackBFramesBSFContext;

/* search next start code */
static unsigned int find_startcode(const uint8_t *buf, int buf_size, int *pos)
{
    unsigned int startcode = 0xFF;

    for (; *pos < buf_size;) {
        startcode = ((startcode << 8) | buf[*pos]) & 0xFFFFFFFF;
        *pos +=1;
        if ((startcode & 0xFFFFFF00) != 0x100)
            continue;  /* no startcode */
        return startcode;
    }

    return 0;
}

/* determine the position of the packed marker in the userdata,
 * the number of VOPs and the position of the second VOP */
static void scan_buffer(const uint8_t *buf, int buf_size,
                        int *pos_p, int *nb_vop, int *pos_vop2) {
    unsigned int startcode;
    int pos, i;

    for (pos = 0; pos < buf_size;) {
        startcode = find_startcode(buf, buf_size, &pos);

        if (startcode == USER_DATA_STARTCODE && pos_p) {
            /* check if the (DivX) userdata string ends with 'p' (packed) */
            for (i = 0; i < 255 && pos + i + 1 < buf_size; i++) {
                if (buf[pos + i] == 'p' && buf[pos + i + 1] == '\0') {
                    *pos_p = pos + i;
                    break;
                }
            }
        } else if (startcode == VOP_STARTCODE && nb_vop) {
            *nb_vop += 1;
            if (*nb_vop == 2 && pos_vop2) {
                *pos_vop2 = pos - 4; /* subtract 4 bytes startcode */
            }
        }
    }
}

static int mpeg4_unpack_bframes_filter(AVBSFContext *ctx, AVPacket *out)
{
    UnpackBFramesBSFContext *s = ctx->priv_data;
    int pos_p = -1, nb_vop = 0, pos_vop2 = -1, ret = 0;
    AVPacket *in;

    ret = ff_bsf_get_packet(ctx, &in);
    if (ret < 0)
        return ret;

    scan_buffer(in->data, in->size, &pos_p, &nb_vop, &pos_vop2);
    av_log(ctx, AV_LOG_DEBUG, "Found %d VOP startcode(s) in this packet.\n", nb_vop);

    if (pos_vop2 >= 0) {
        if (s->b_frame->data) {
            av_log(ctx, AV_LOG_WARNING,
                   "Missing one N-VOP packet, discarding one B-frame.\n");
            av_packet_unref(s->b_frame);
        }
        /* store the packed B-frame in the BSFContext */
        ret = av_packet_ref(s->b_frame, in);
        if (ret < 0) {
            goto fail;
        }
        s->b_frame->size -= pos_vop2;
        s->b_frame->data += pos_vop2;
    }

    if (nb_vop > 2) {
        av_log(ctx, AV_LOG_WARNING,
       "Found %d VOP headers in one packet, only unpacking one.\n", nb_vop);
    }

    if (nb_vop == 1 && s->b_frame->data) {
        /* use frame from BSFContext */
        av_packet_move_ref(out, s->b_frame);

        /* use properties from current input packet */
        ret = av_packet_copy_props(out, in);
        if (ret < 0) {
            goto fail;
        }

        if (in->size <= MAX_NVOP_SIZE) {
            /* N-VOP */
            av_log(ctx, AV_LOG_DEBUG, "Skipping N-VOP.\n");
        } else {
            /* copy packet into BSFContext */
            av_packet_move_ref(s->b_frame, in);
        }
    } else if (nb_vop >= 2) {
        /* use first frame of the packet */
        av_packet_move_ref(out, in);
        out->size = pos_vop2;
    } else if (pos_p >= 0) {
        ret = av_packet_make_writable(in);
        if (ret < 0)
            goto fail;
        av_log(ctx, AV_LOG_DEBUG, "Updating DivX userdata (remove trailing 'p').\n");
        av_packet_move_ref(out, in);
        /* remove 'p' (packed) from the end of the (DivX) userdata string */
        out->data[pos_p] = '\0';
    } else {
        /* copy packet */
        av_packet_move_ref(out, in);
    }

fail:
    if (ret < 0)
        av_packet_unref(out);
    av_packet_free(&in);

    return ret;
}

static int mpeg4_unpack_bframes_init(AVBSFContext *ctx)
{
    UnpackBFramesBSFContext *s = ctx->priv_data;

    s->b_frame = av_packet_alloc();
    if (!s->b_frame)
        return AVERROR(ENOMEM);

    if (ctx->par_in->extradata) {
        int pos_p_ext = -1;
        scan_buffer(ctx->par_in->extradata, ctx->par_in->extradata_size, &pos_p_ext, NULL, NULL);
        if (pos_p_ext >= 0) {
            av_log(ctx, AV_LOG_DEBUG,
                   "Updating DivX userdata (remove trailing 'p') in extradata.\n");
            ctx->par_out->extradata[pos_p_ext] = '\0';
        }
    }

    return 0;
}

static void mpeg4_unpack_bframes_flush(AVBSFContext *bsfc)
{
    UnpackBFramesBSFContext *ctx = bsfc->priv_data;
    av_packet_unref(ctx->b_frame);
}

static void mpeg4_unpack_bframes_close(AVBSFContext *bsfc)
{
    UnpackBFramesBSFContext *ctx = bsfc->priv_data;
    av_packet_free(&ctx->b_frame);
}

static const enum AVCodecID codec_ids[] = {
    AV_CODEC_ID_MPEG4, AV_CODEC_ID_NONE,
};

const AVBitStreamFilter ff_mpeg4_unpack_bframes_bsf = {
    .name           = "mpeg4_unpack_bframes",
    .priv_data_size = sizeof(UnpackBFramesBSFContext),
    .init           = mpeg4_unpack_bframes_init,
    .filter         = mpeg4_unpack_bframes_filter,
    .flush          = mpeg4_unpack_bframes_flush,
    .close          = mpeg4_unpack_bframes_close,
    .codec_ids      = codec_ids,
};
