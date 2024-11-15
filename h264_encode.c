#include "h264_encode.h"
#include <pthread.h>
#include "libyuv_scale.h"

struct plane_start {
    void *start;
};

struct buffer {
    struct plane_start *plane_start;
    struct v4l2_plane *planes_buffer;
};

void h264_encode_init(struct H264EncodeParameter *h264EncodeParameter) {
    // 初始化x264参数
    x264_param_default_preset(&h264EncodeParameter->param, "veryfast", "zerolatency");
    h264EncodeParameter->param.i_width = h264EncodeParameter->width_h264;
    h264EncodeParameter->param.i_height = h264EncodeParameter->height_h264;
    h264EncodeParameter->param.i_fps_num = 30;
    h264EncodeParameter->param.i_fps_den = 1;
    h264EncodeParameter->param.i_csp = X264_CSP_NV12; // 设置颜色空间为 NV12
    h264EncodeParameter->param.b_full_recon = 0;      // 不使用完整颜色范围（对应 --input-range tv）

    // 打开x264编码器
    h264EncodeParameter->encoder = x264_encoder_open(&h264EncodeParameter->param);
    if (!h264EncodeParameter->encoder) {
        printf("无法打开x264编码器！\n");
        return;
    }

    // 初始化x264输入/输出图像结构
    x264_picture_alloc(&h264EncodeParameter->pic_in, X264_CSP_NV12, h264EncodeParameter->width_h264,
                       h264EncodeParameter->height_h264);
    x264_picture_init(&h264EncodeParameter->pic_out);

    // 每帧的YUV大小
    h264EncodeParameter->y_size = h264EncodeParameter->width_h264 * h264EncodeParameter->height_h264;
    h264EncodeParameter->uv_size = h264EncodeParameter->y_size / 2;  // NV12格式的UV平面是交错的，所以UV占用总像素数的一半
    h264EncodeParameter->frame_num = 0;
}

int h264_encode(struct EncodeParameter *encodeParameter) {
    // 计算输出帧大小
    encodeParameter->frame_size_scale = encodeParameter->width_scale * encodeParameter->height_scale +
                                        (encodeParameter->width_scale / 2) * (encodeParameter->height_scale / 2) *
                                        2;
    //h264
    struct H264EncodeParameter h264EncodeParameter;
    h264EncodeParameter.width_h264 = encodeParameter->width;
    h264EncodeParameter.height_h264 = encodeParameter->height;
    h264_encode_init(&h264EncodeParameter);

    struct H264EncodeParameter h264EncodeParameter_scale;
    h264EncodeParameter_scale.width_h264 = encodeParameter->width_scale;
    h264EncodeParameter_scale.height_h264 = encodeParameter->height_scale;
    h264_encode_init(&h264EncodeParameter_scale);

    int fd;
    fd_set fds;
    FILE *file_fd, *file_fd_scale, *file_fd_h264, *file_fd_h264_scale;
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

    fd = open(encodeParameter->input_file, O_RDWR);
    if (fd < 0) {
        printf("open device: %s fail\n", encodeParameter->input_file);
        goto err;
    }

#ifdef OUTPUT_FILE
    file_fd = fopen(encodeParameter->output_file, "wb+");
    if (!file_fd) {
        printf("open save_file: %s fail\n", encodeParameter->output_file);
        goto err1;
    }
#endif

#ifdef OUTPUT_FILE_SCALE
    file_fd_scale = fopen(encodeParameter->output_file_scale, "wb+");
    if (!file_fd_scale) {
        printf("open save_file: %s fail\n", encodeParameter->output_file_scale);
        goto err1;
    }
#endif
#ifdef OUTPUT_FILE_H264
    file_fd_h264 = fopen(encodeParameter->output_file_h264, "wb+");
    if (!file_fd_h264) {
        printf("open save_file: %s fail\n", encodeParameter->output_file_h264);
        goto err1;
    }
#endif
#ifdef OUTPUT_FILE_H264_SCALE
    file_fd_h264_scale = fopen(encodeParameter->output_file_h264_scale, "wb+");
    if (!file_fd_h264_scale) {
        printf("open save_file: %s fail\n", encodeParameter->output_file_h264_scale);
        goto err1;
    }
#endif
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
    fmt.fmt.pix_mp.width = encodeParameter->width;
    fmt.fmt.pix_mp.height = encodeParameter->height;
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
            goto err2;
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
            encodeParameter->buffer_start = ((buffers + buf.index)->plane_start + j)->start;
            encodeParameter->buffer_bytesused = (tmp_plane + j)->bytesused;

#ifdef OUTPUT_FILE
            //写入yuv文件
            fwrite(encodeParameter->buffer_start, encodeParameter->buffer_bytesused, 1, file_fd);
#endif

            int frame_size = yuv_to_h264(encodeParameter->buffer_start, &h264EncodeParameter);
            if (frame_size > 0) {
#ifdef OUTPUT_FILE_H264
                // 将编码后的NAL单元写入文件
                fwrite(h264EncodeParameter.nals[0].p_payload, 1, frame_size, file_fd_h264);
#endif
                h264_send_ch(encodeParameter->chn_id, h264EncodeParameter.nals[0].p_payload, frame_size);
            }


            //缩放NV12
            uint8_t *dst_nv12 = (uint8_t *) malloc(encodeParameter->frame_size_scale);

            ScaleNV12(encodeParameter->buffer_start, encodeParameter->width, encodeParameter->height,
                      dst_nv12, encodeParameter->width_scale, encodeParameter->height_scale);

#ifdef OUTPUT_FILE_SCALE
            //写入缩放文件
            fwrite(dst_nv12, 1, encodeParameter->frame_size_scale, file_fd_scale);
#endif
            int frame_size_scale = yuv_to_h264(dst_nv12, &h264EncodeParameter_scale);
            if (frame_size_scale > 0) {
#ifdef OUTPUT_FILE_H264
                // 将编码后的NAL单元写入文件
                fwrite(h264EncodeParameter_scale.nals[0].p_payload, 1, frame_size_scale, file_fd_h264_scale);
#endif
                h264_send_ch(encodeParameter->chn_id + 1, h264EncodeParameter_scale.nals[0].p_payload,
                             frame_size_scale);
            }
            free(dst_nv12);
        }

#ifdef OUTPUT_FRAME_NUM
        num++;
        if (num >= OUTPUT_FRAME_NUM)
            break;
#endif

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

    // 清理和关闭
    x264_picture_clean(&h264EncodeParameter.pic_in);
    x264_encoder_close(h264EncodeParameter.encoder);

    x264_picture_clean(&h264EncodeParameter_scale.pic_in);
    x264_encoder_close(h264EncodeParameter_scale.encoder);
#ifdef OUTPUT_FILE_H264_SCALE
    fclose(file_fd_h264_scale);
#endif
#ifdef OUTPUT_FILE_H264
    fclose(file_fd_h264);
#endif
#ifdef OUTPUT_FILE_SCALE
    fclose(file_fd_scale);
#endif
#ifdef OUTPUT_FILE
    fclose(file_fd);
#endif
    err1:
    close(fd);
    err:
    return ret;
}

int yuv_to_h264(void *buffer, struct H264EncodeParameter *h264EncodeParameter) {
    //yuv转h264
    memcpy(h264EncodeParameter->pic_in.img.plane[0], buffer, h264EncodeParameter->y_size);
    memcpy(h264EncodeParameter->pic_in.img.plane[1], buffer + h264EncodeParameter->y_size,
           h264EncodeParameter->uv_size);
    h264EncodeParameter->pic_in.i_pts = h264EncodeParameter->frame_num++;
    // 编码帧
    return x264_encoder_encode(h264EncodeParameter->encoder, &h264EncodeParameter->nals,
                               &h264EncodeParameter->i_nals,
                               &h264EncodeParameter->pic_in, &h264EncodeParameter->pic_out);
}

void h264_send_ch(u_int8_t id, void *pVoid, size_t len) {

}

int main(int argc, char **argv) {
    struct EncodeParameter encodeParameter[4];
    uint8_t i = 0;
    encodeParameter[i].pid = i;
    encodeParameter[i].chn_id = i;
    //v4l2
    encodeParameter[i].input_file = "/dev/video11";
    encodeParameter[i].output_file = "output.yuv";
    encodeParameter[i].output_file_scale = "output_s.yuv";
    encodeParameter[i].output_file_h264 = "output.h264";
    encodeParameter[i].output_file_h264_scale = "output_s.h264";
    encodeParameter[i].width = 1280;
    encodeParameter[i].height = 720;
    //yuv
    encodeParameter[i].scale = 1;
    encodeParameter[i].width_scale = 640;
    encodeParameter[i].height_scale = 480;
    pthread_create(&encodeParameter[i].pid, NULL, h264_encode, &encodeParameter[i]);

    while (1) {
        usleep(10 * 1000);
    }
}
