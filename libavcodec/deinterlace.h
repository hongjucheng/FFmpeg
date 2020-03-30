/*
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

#ifndef AVCODEC_DEINTERLACE_H
#define AVCODEC_DEINTERLACE_H

#include <stdint.h>

#include "version.h"

void ff_deinterlace_line_mmx(uint8_t *dst,
                             const uint8_t *lum_m4, const uint8_t *lum_m3,
                             const uint8_t *lum_m2, const uint8_t *lum_m1,
                             const uint8_t *lum,
                             int size);

void ff_deinterlace_line_inplace_mmx(const uint8_t *lum_m4,
                                     const uint8_t *lum_m3,
                                     const uint8_t *lum_m2,
                                     const uint8_t *lum_m1,
                                     const uint8_t *lum, int size);

#endif /* AVCODEC_DEINTERLACE_H */                 