//
// Created by zjw93 on 2024/11/14.
//

#ifndef V4L2_H264_ENCODE_H
#define V4L2_H264_ENCODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdbool.h>

struct encodeParameter {
    pthread_t pid;
    u_int8_t chn_id;
    //v4l2
    char *input_file;
    char *output_file;
    u_int32_t width;
    u_int32_t height;
    void *buffer_start;
    int buffer_bytesused;
    //libyuv
    bool scale;
    u_int32_t width_scale;
    u_int32_t height_scale;
    u_int32_t frame_size_scale;
    char *output_file_scale;
    //xh264
    char *output_file_h264;
};

#define OUTPUT_FRAME_NUM 100
#define OUTPUT_FILE
#define OUTPUT_FILE_SCALE
#define OUTPUT_FILE_H264

#endif //V4L2_H264_ENCODE_H
