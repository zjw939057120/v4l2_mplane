#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libyuv.h>

int main() {
    // 原始YUV文件路径和缩小后YUV文件路径
    const char* input_filename = "test.yuv";
    const char* output_filename = "test_s.yuv";

    // 原始和目标分辨率
    int src_width = 1280;
    int src_height = 720;
    int dst_width = 640;
    int dst_height = 480;

    // 计算 YUV 文件的大小
    int src_y_size = src_width * src_height;
    int src_uv_size = src_y_size / 4;
    int dst_y_size = dst_width * dst_height;
    int dst_uv_size = dst_y_size / 4;

    // 分配内存
    uint8_t *src_yuv = (uint8_t *)malloc(src_y_size + 2 * src_uv_size);
    uint8_t *dst_yuv = (uint8_t *)malloc(dst_y_size + 2 * dst_uv_size);

    if (!src_yuv || !dst_yuv) {
        fprintf(stderr, "Memory allocation failed.\n");
        return -1;
    }

    // 读取源YUV数据
    FILE *input_file = fopen(input_filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Failed to open input YUV file.\n");
        free(src_yuv);
        free(dst_yuv);
        return -1;
    }

    fread(src_yuv, 1, src_y_size + 2 * src_uv_size, input_file);
    fclose(input_file);

    // YUV分量
    uint8_t *src_y = src_yuv;
    uint8_t *src_u = src_y + src_y_size;
    uint8_t *src_v = src_u + src_uv_size;

    uint8_t *dst_y = dst_yuv;
    uint8_t *dst_u = dst_y + dst_y_size;
    uint8_t *dst_v = dst_u + dst_uv_size;

    // 步幅（stride）
    int src_stride_y = src_width;
    int src_stride_u = src_width / 2;
    int src_stride_v = src_width / 2;

    int dst_stride_y = dst_width;
    int dst_stride_u = dst_width / 2;
    int dst_stride_v = dst_width / 2;

    // 使用 libyuv 进行缩小
    int result = libyuv::I420Scale(
            src_y, src_stride_y,
            src_u, src_stride_u,
            src_v, src_stride_v,
            src_width, src_height,
            dst_y, dst_stride_y,
            dst_u, dst_stride_u,
            dst_v, dst_stride_v,
            dst_width, dst_height,
            libyuv::kFilterNone  // 无滤波器
    );

    if (result != 0) {
        fprintf(stderr, "I420Scale failed with error code %d.\n", result);
        free(src_yuv);
        free(dst_yuv);
        return -1;
    }

    // 将缩小后的YUV数据写入文件
    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        fprintf(stderr, "Failed to open output YUV file.\n");
        free(src_yuv);
        free(dst_yuv);
        return -1;
    }

    fwrite(dst_yuv, 1, dst_y_size + 2 * dst_uv_size, output_file);
    fclose(output_file);

    printf("YUV file scaled successfully!\n");

    // 释放内存
    free(src_yuv);
    free(dst_yuv);

    return 0;
}
