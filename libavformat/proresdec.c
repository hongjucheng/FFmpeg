/*
 * RAW ProRes demuxer
 * Copyright (c) 2008 Baptiste Coudurier <baptiste.coudurier@gmail.com>
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

#include "libavutil/intreadwrite.h"
#include "avformat.h"
#include "rawdec.h"

static int prores_probe(const AVProbeData *p)
{
    const uint8_t *buf;
    int buf_size, hdr_size, version;
    int width, height, frame_type, alpha_info;

    if (p->buf_size < 28 || AV_RL32(p->buf + 4) != AV_RL32("icpf"))
        return 0;

    buf      = p->buf + 8;
    buf_size = p->buf_size - 8;

    hdr_size = AV_RB16(buf);
    version  = AV_RB16(buf + 2);
    if (version > 1)
        return 0;

    width  = AV_RB16(buf + 8);
    height = AV_RB16(buf + 10);
    if (!width || !height)
        return 0;

    frame_type = (buf[12] >> 2) & 3;
    if (frame_type < 0 || frame_type > 2)
        return 0;
    alpha_info = buf[17] & 0xf;
    if (alpha_info > 2)
        return 0;

    return AVPROBE_SCORE_MAX;
}

FF_DEF_RAWVIDEO_DEMUXER(prores, "raw ProRes", prores_probe, NULL, AV_CODEC_ID_PRORES)
