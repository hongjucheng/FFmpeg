/*
 * ProRes parser
 * Copyright (c) 2019 hongjucheng <hongjucheng_17@163.com>
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

#include "parser.h"
#include "libavutil/intreadwrite.h"

static int prores_find_frame_end(ParseContext *pc, const uint8_t *buf, int buf_size)
{
    int pic_found  = pc->frame_start_found;
    uint32_t state = pc->state;
    int cur = 0;

    int flag = AV_RB32("icpf");
    if (!pic_found) {
        for (; cur < buf_size; cur++) {
            state = (state<<8) | buf[cur];
            if (state == flag){
                ++cur;
                pic_found = 1;
                break;
            }
        }
    }

    if (pic_found) {
        if (!buf_size)
            return END_NOT_FOUND;
        for (; cur < buf_size; ++cur) {
            state = (state << 8) | buf[cur];
            if (state == flag) {
                pc->frame_start_found = 0;
                pc->state = -1;
                return cur - 7;
            }
        }
    }

    pc->frame_start_found = pic_found;
    pc->state = state;

    return END_NOT_FOUND;
}

static int prores_parse(AVCodecParserContext *s, AVCodecContext *avctx,
                      const uint8_t **poutbuf, int *poutbuf_size,
                      const uint8_t *buf, int buf_size)
{
    ParseContext *pc = s->priv_data;
    int next;

    if (s->flags & PARSER_FLAG_COMPLETE_FRAMES)  {
        next = buf_size;
    } else {
        next = prores_find_frame_end(pc, buf, buf_size);
        if (ff_combine_frame(pc, next, &buf, &buf_size) < 0) {
            *poutbuf = NULL;
            *poutbuf_size = 0;
            return buf_size;
        }
    }

    *poutbuf = buf;
    *poutbuf_size = buf_size;

    return next;
}

AVCodecParser ff_prores_parser = {
    .codec_ids      = { AV_CODEC_ID_PRORES },
    .priv_data_size = sizeof(ParseContext),
    .parser_parse   = prores_parse,
    .parser_close   = ff_parse_close
};
