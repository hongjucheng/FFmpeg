/*
 * RAW ProRes demuxer
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

#include "libavutil/intreadwrite.h"
#include "avformat.h"
#include "rawdec.h"

static int prores_check_frame_header(const uint8_t *buf, const int data_size)
{
    int hdr_size, width, height;
    int version, alpha_info;

    hdr_size = AV_RB16(buf);
    if (hdr_size < 20)
        return AVERROR_INVALIDDATA;

    version = buf[3];
    if (version > 1)
        return AVERROR_INVALIDDATA;

    width  = AV_RB16(buf + 8);
    height = AV_RB16(buf + 10);
    if (!width || !height)
        return AVERROR_INVALIDDATA;

    alpha_info    = buf[17] & 0x0f;
    if (alpha_info > 2)
        return AVERROR_INVALIDDATA;

    return 0;
}

static int prores_probe(const AVProbeData *p)
{
    if (p->buf_size < 28 || AV_RL32(p->buf + 4) != AV_RL32("icpf"))
        return 0;

    if (prores_check_frame_header(p->buf + 8, p->buf_size - 8) < 0)
        return 0;

    return AVPROBE_SCORE_MAX;
}

FF_DEF_RAWVIDEO_DEMUXER(prores, "raw ProRes", prores_probe, NULL, AV_CODEC_ID_PRORES)
