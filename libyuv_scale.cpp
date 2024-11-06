#include <stdio.h>
#include <stdlib.h>
#include <libyuv.h>

// 打开YUV文件，读取数据并返回指针
uint8_t* ReadYUVFile(const char* filename, int width, int height, int* frame_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("无法打开文件: %s\n", filename);
        return nullptr;
    }

    // NV12 的帧大小 = Y平面大小 + UV平面大小
    *frame_size = width * height + (width / 2) * (height / 2) * 2;
    uint8_t* buffer = new uint8_t[*frame_size];

    // 读取 YUV 数据
    fread(buffer, 1, *frame_size, file);
    fclose(file);

    return buffer;
}

// 保存处理后的YUV数据到文件
void WriteYUVFile(const char* filename, uint8_t* buffer, int frame_size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("无法保存文件: %s\n", filename);
        return;
    }

    fwrite(buffer, 1, frame_size, file);
    fclose(file);
}

// 使用libyuv缩放NV12格式的数据
void ScaleNV12(const uint8_t* src_nv12, int src_width, int src_height,
               uint8_t* dst_nv12, int dst_width, int dst_height) {
    // 计算输入/输出图像的缓冲区大小
    int src_y_size = src_width * src_height;
    int src_uv_size = (src_width / 2) * (src_height / 2) * 2;

    int dst_y_size = dst_width * dst_height;
    int dst_uv_size = (dst_width / 2) * (dst_height / 2) * 2;

    // 临时缓冲区，用于 I420 格式的转换
    uint8_t* src_i420_y = new uint8_t[src_y_size];
    uint8_t* src_i420_u = new uint8_t[src_y_size / 4];
    uint8_t* src_i420_v = new uint8_t[src_y_size / 4];

    uint8_t* dst_i420_y = new uint8_t[dst_y_size];
    uint8_t* dst_i420_u = new uint8_t[dst_y_size / 4];
    uint8_t* dst_i420_v = new uint8_t[dst_y_size / 4];

    // NV12 转换为 I420
    libyuv::NV12ToI420(src_nv12, src_width,
                       src_nv12 + src_y_size, src_width,
                       src_i420_y, src_width,
                       src_i420_u, src_width / 2,
                       src_i420_v, src_width / 2,
                       src_width, src_height);

    // 缩放 I420 格式
    libyuv::I420Scale(src_i420_y, src_width,
                      src_i420_u, src_width / 2,
                      src_i420_v, src_width / 2,
                      src_width, src_height,
                      dst_i420_y, dst_width,
                      dst_i420_u, dst_width / 2,
                      dst_i420_v, dst_width / 2,
                      dst_width, dst_height,
                      libyuv::kFilterBox);  // 使用 box 滤波器

    // 将缩小后的 I420 转换回 NV12
    libyuv::I420ToNV12(dst_i420_y, dst_width,
                       dst_i420_u, dst_width / 2,
                       dst_i420_v, dst_width / 2,
                       dst_nv12, dst_width,
                       dst_nv12 + dst_y_size, dst_width,
                       dst_width, dst_height);

    // 清理内存
    delete[] src_i420_y;
    delete[] src_i420_u;
    delete[] src_i420_v;
    delete[] dst_i420_y;
    delete[] dst_i420_u;
    delete[] dst_i420_v;
}

int main() {
    const char* input_file = "test.yuv";
    const char* output_file = "test_s.yuv";

    // 输入图像的宽高
    int src_width = 1280;
    int src_height = 720;

    // 输出图像的宽高
    int dst_width = 640;
    int dst_height = 480;

    // 读取输入 YUV 文件
    int src_frame_size;
    uint8_t* src_nv12 = ReadYUVFile(input_file, src_width, src_height, &src_frame_size);
    if (!src_nv12) {
        printf("读取输入文件失败!\n");
        return -1;
    }

    // 计算输出帧大小
    int dst_frame_size = dst_width * dst_height + (dst_width / 2) * (dst_height / 2) * 2;
    uint8_t* dst_nv12 = new uint8_t[dst_frame_size];

    // 缩放 NV12
    ScaleNV12(src_nv12, src_width, src_height, dst_nv12, dst_width, dst_height);

    // 保存输出 YUV 文件
    WriteYUVFile(output_file, dst_nv12, dst_frame_size);

    // 释放内存
    delete[] src_nv12;
    delete[] dst_nv12;

    printf("处理完成，输出文件: %s\n", output_file);
    return 0;
}
