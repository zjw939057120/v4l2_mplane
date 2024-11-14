//
// Created by zjw93 on 2024/11/14.
//

#ifndef V4L2_LIBYUV_SCALE_H
#define V4L2_LIBYUV_SCALE_H

#include <stdio.h>
#include <stdlib.h>
#include <libyuv.h>

void ScaleNV12(const uint8_t *src_nv12, int src_width, int src_height,
               uint8_t *dst_nv12, int dst_width, int dst_height);

#endif //V4L2_LIBYUV_SCALE_H
