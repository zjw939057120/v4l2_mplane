#include "h264_encode.h"
#include "libyuv_scale.h"
#include <x264.h>

struct plane_start {
    void *start;
};

struct buffer {
    struct plane_start *plane_start;
    struct v4l2_plane *planes_buffer;
};

int main(int argc, char **argv) {
    struct encodeParameter encodeParameters;
    //v4l2
    encodeParameters.input_file = "/dev/video11";
    encodeParameters.output_file = "output.yuv";
    encodeParameters.width = 1280;
    encodeParameters.height = 720;
    encodeParameters.frame_num = 2000;
    //yuv
    encodeParameters.scale = 1;
    encodeParameters.width_scale = 640;
    encodeParameters.height_scale = 480;
    // 计算输出帧大小
    encodeParameters.frame_size_scale = encodeParameters.width_scale * encodeParameters.height_scale +
                                        (encodeParameters.width_scale / 2) * (encodeParameters.height_scale / 2) *
                                        2;
    encodeParameters.output_file_scale = "output_s.yuv";
    //h264
    u_int32_t width_h264, height_h264;
    if (encodeParameters.scale) {
        width_h264 = encodeParameters.width_scale;
        height_h264 = encodeParameters.height_scale;
        encodeParameters.output_file_h264 = "output_s.h264";
    } else {
        width_h264 = encodeParameters.width;
        height_h264 = encodeParameters.height;
        encodeParameters.output_file_h264 = "output.h264";
    }

    // 初始化x264参数
    x264_param_t param;
    x264_param_default_preset(&param, "veryfast", "zerolatency");
    param.i_width = width_h264;
    param.i_height = height_h264;
    param.i_fps_num = 30;
    param.i_fps_den = 1;
    param.i_csp = X264_CSP_NV12; // 设置颜色空间为 NV12
    param.b_full_recon = 0;      // 不使用完整颜色范围（对应 --input-range tv）

    // 打开x264编码器
    x264_t *encoder = x264_encoder_open(&param);
    if (!encoder) {
        printf("无法打开x264编码器！\n");
//        fclose(yuv_file);
//        fclose(h264_file);
        return -1;
    }

    // 初始化x264输入/输出图像结构
    x264_picture_t pic_in, pic_out;
    x264_picture_alloc(&pic_in, X264_CSP_NV12, width_h264, height_h264);
    x264_picture_init(&pic_out);

    // 每帧的YUV大小
    int y_size = width_h264 * height_h264;
    int uv_size = y_size / 2;  // NV12格式的UV平面是交错的，所以UV占用总像素数的一半

    // 编码过程
    int frame_num = 0;
    x264_nal_t *nals;
    int i_nals;

    int fd;
    fd_set fds;
    FILE *file_fd, *file_fd_scale, *file_fd_h264;
    struct timeval tv;
    int ret = -1, i, j, r;
    int num_planes;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    struct v4l2_plane *planes_buffer;
    struct plane_start *plane_start;
    struct buffer *buffers;
    enum v4l2_buf_type type;

    fd = open(encodeParameters.input_file, O_RDWR);
    if (fd < 0) {
        printf("open device: %s fail\n", encodeParameters.input_file);
        goto err;
    }

    file_fd = fopen(encodeParameters.output_file, "wb+");
    if (!file_fd) {
        printf("open save_file: %s fail\n", encodeParameters.output_file);
        goto err1;
    }

    file_fd_scale = fopen(encodeParameters.output_file_scale, "wb+");
    if (!file_fd_scale) {
        printf("open save_file: %s fail\n", encodeParameters.output_file_scale);
        goto err1;
    }
    file_fd_h264 = fopen(encodeParameters.output_file_h264, "wb+");
    if (!file_fd_h264) {
        printf("open save_file: %s fail\n", encodeParameters.output_file_h264);
        goto err1;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        printf("Get video capability error!\n");
        goto err1;
    }

    if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        printf("Video device not support capture!\n");
        goto err1;
    }

    printf("Support capture!\n");

    memset(&fmt, 0, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = encodeParameters.width;
    fmt.fmt.pix_mp.height = encodeParameters.height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        printf("Set format fail\n");
        goto err1;
    }

    printf("width = %d\n", fmt.fmt.pix_mp.width);
    printf("height = %d\n", fmt.fmt.pix_mp.height);
    printf("nmplane = %d\n", fmt.fmt.pix_mp.num_planes);

    //memset(&fmt, 0, sizeof(struct v4l2_format));
    //fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    //if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
    //	printf("Set format fail\n");
    //	goto err;
    //}

    //printf("nmplane = %d\n", fmt.fmt.pix_mp.num_planes);

    req.count = 5;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        printf("Reqbufs fail\n");
        goto err1;
    }

    printf("buffer number: %d\n", req.count);

    num_planes = fmt.fmt.pix_mp.num_planes;

    buffers = malloc(req.count * sizeof(*buffers));

    for (i = 0; i < req.count; i++) {
        memset(&buf, 0, sizeof(buf));
        planes_buffer = calloc(num_planes, sizeof(*planes_buffer));
        plane_start = calloc(num_planes, sizeof(*plane_start));
        memset(planes_buffer, 0, sizeof(*planes_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes_buffer;
        buf.length = num_planes;
        buf.index = i;
        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            printf("Querybuf fail\n");
            req.count = i;
            goto err2;
        }

        (buffers + i)->planes_buffer = planes_buffer;
        (buffers + i)->plane_start = plane_start;
        for (j = 0; j < num_planes; j++) {
            printf("plane[%d]: length = %d\n", j, (planes_buffer + j)->length);
            printf("plane[%d]: offset = %d\n", j, (planes_buffer + j)->m.mem_offset);
            (plane_start + j)->start = mmap(NULL /* start anywhere */,
                                            (planes_buffer + j)->length,
                                            PROT_READ | PROT_WRITE /* required */,
                                            MAP_SHARED /* recommended */,
                                            fd,
                                            (planes_buffer + j)->m.mem_offset);
            if (MAP_FAILED == (plane_start + j)->start) {
                printf("mmap failed\n");
                req.count = i;
                goto unmmap;
            }
        }
    }

    for (i = 0; i < req.count; ++i) {
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = num_planes;
        buf.index = i;
        buf.m.planes = (buffers + i)->planes_buffer;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
            printf("VIDIOC_QBUF failed\n");
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
        printf("VIDIOC_STREAMON failed\n");

    int num = 0;
    struct v4l2_plane *tmp_plane;
    tmp_plane = calloc(num_planes, sizeof(*tmp_plane));

    while (1) {
        FD_ZERO (&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        r = select(fd + 1, &fds, NULL, NULL, &tv);

        if (-1 == r) {
            if (EINTR == errno)
                continue;
            printf("select err\n");
        }
        if (0 == r) {
            fprintf(stderr, "select timeout\n");
            exit(EXIT_FAILURE);
        }

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = tmp_plane;
        buf.length = num_planes;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
            printf("dqbuf fail\n");

        for (j = 0; j < num_planes; j++) {
            printf("plane[%d] start = %p, bytesused = %d\n", j, ((buffers + buf.index)->plane_start + j)->start,
                   (tmp_plane + j)->bytesused);
            encodeParameters.buffer_start = ((buffers + buf.index)->plane_start + j)->start;
            encodeParameters.buffer_bytesused = (tmp_plane + j)->bytesused;

            //写入yuv文件
            fwrite(encodeParameters.buffer_start, encodeParameters.buffer_bytesused, 1, file_fd);

            //缩放NV12
            uint8_t *dst_nv12 = (uint8_t *) malloc(encodeParameters.frame_size_scale);

            ScaleNV12(encodeParameters.buffer_start, encodeParameters.width, encodeParameters.height,
                      dst_nv12, encodeParameters.width_scale, encodeParameters.height_scale);

            //写入缩放文件
            fwrite(dst_nv12, 1, encodeParameters.frame_size_scale, file_fd_scale);

            //yuv转h264
            memcpy(pic_in.img.plane[0], dst_nv12, y_size);
            memcpy(pic_in.img.plane[1], dst_nv12 + y_size, uv_size);
            pic_in.i_pts = frame_num++;
            // 编码帧
            int frame_size = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);
            if (frame_size > 0) {
                // 将编码后的NAL单元写入文件
                fwrite(nals[0].p_payload, 1, frame_size, file_fd_h264);
            }
            free(dst_nv12);
        }

        num++;
        if (num >= encodeParameters.frame_num)
            break;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
            printf("failture VIDIOC_QBUF\n");
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
        printf("VIDIOC_STREAMOFF fail\n");

    free(tmp_plane);

    ret = 0;
    unmmap:
    err2:
    for (i = 0; i < req.count; i++) {
        for (j = 0; j < num_planes; j++) {
            if (MAP_FAILED != ((buffers + i)->plane_start + j)->start) {
                if (-1 == munmap(((buffers + i)->plane_start + j)->start, ((buffers + i)->planes_buffer + j)->length))
                    printf("munmap error\n");
            }
        }
    }

    for (i = 0; i < req.count; i++) {
        free((buffers + i)->planes_buffer);
        free((buffers + i)->plane_start);
    }

    free(buffers);

    fclose(file_fd_h264);
    fclose(file_fd_scale);
    fclose(file_fd);
    err1:
    close(fd);
    err:
    return ret;
}
