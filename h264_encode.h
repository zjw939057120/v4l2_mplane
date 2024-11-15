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
#include <stdint.h>
#include <x264.h>

struct EncodeParameter {
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
    //h264
    char *output_file_h264;
    char *output_file_h264_scale;
};

struct H264EncodeParameter {
    int width_h264, height_h264;
    x264_param_t param;
    x264_t *encoder;
    x264_picture_t pic_in, pic_out;
    int y_size, uv_size;
    // 编码过程
    int64_t frame_num;
    x264_nal_t *nals;
    int i_nals;
};

void h264_encode_init(struct H264EncodeParameter *h264EncodeParameter);

int h264_encode(struct EncodeParameter *encodeParameter);

int yuv_to_h264(void *buffer, struct H264EncodeParameter *h264EncodeParameter);

void h264_send_ch(u_int8_t id, void *pVoid, size_t len);

u_int8_t stream_type_video_h264 = 0;

#define OUTPUT_FRAME_NUM 100
#define OUTPUT_FILE
#define OUTPUT_FILE_SCALE
#define OUTPUT_FILE_H264
#define OUTPUT_FILE_H264_SCALE

#endif //V4L2_H264_ENCODE_H
