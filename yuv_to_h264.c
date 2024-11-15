#include <stdio.h>
#include <sys/mman.h>
#include <x264.h>

int main(int argc, char **argv) {
    // 参数设置
    int width = 640;
    int height = 480;
    int fps = 30;

    // 打开YUV文件
    FILE *yuv_file = fopen("output_s.yuv", "rb");
    if (!yuv_file) {
        printf("无法打开YUV文件！\n");
        return -1;
    }

    // 打开输出H.264文件
    FILE *h264_file = fopen("output_s.h264", "wb");
    if (!h264_file) {
        printf("无法打开H.264文件！\n");
        fclose(yuv_file);
        return -1;
    }

    // 初始化x264参数
    x264_param_t param;
    x264_param_default_preset(&param, "veryfast", "zerolatency");
    param.i_width = width;
    param.i_height = height;
    param.i_fps_num = fps;
    param.i_fps_den = 1;
    param.i_csp = X264_CSP_NV12; // 设置颜色空间为 NV12
    param.b_full_recon = 0;      // 不使用完整颜色范围（对应 --input-range tv）

    // 打开x264编码器
    x264_t *encoder = x264_encoder_open(&param);
    if (!encoder) {
        printf("无法打开x264编码器！\n");
        fclose(yuv_file);
        fclose(h264_file);
        return -1;
    }

    // 初始化x264输入/输出图像结构
    x264_picture_t pic_in, pic_out;
    x264_picture_alloc(&pic_in, X264_CSP_NV12, width, height);
    x264_picture_init(&pic_out);

    // 每帧的YUV大小
    int y_size = width * height;
    int uv_size = y_size / 2;  // NV12格式的UV平面是交错的，所以UV占用总像素数的一半

    // 编码过程
    int frame_num = 0;
    x264_nal_t *nals;
    int i_nals;
    while (fread(pic_in.img.plane[0], 1, y_size, yuv_file) == y_size &&
           fread(pic_in.img.plane[1], 1, uv_size, yuv_file) == uv_size) {

        pic_in.i_pts = frame_num++;

        // 编码帧
        int frame_size = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);

        if (frame_size > 0) {
            // 将编码后的NAL单元写入文件
            fwrite(nals[0].p_payload, 1, frame_size, h264_file);
        }
    }

    // 刷新编码器缓冲区，输出剩余的数据
    while (x264_encoder_encode(encoder, &nals, &i_nals, NULL, &pic_out)) {
        fwrite(nals[0].p_payload, 1, nals[0].i_payload, h264_file);
    }

    // 清理和关闭
    x264_picture_clean(&pic_in);
    x264_encoder_close(encoder);
    fclose(yuv_file);
    fclose(h264_file);

    printf("编码完成！\n");
    return 0;
}
