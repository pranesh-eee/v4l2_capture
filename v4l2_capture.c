#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <getopt.h>

#include "v4l2.h"

int close_video_device(int *fd)
{
    fprintf(stdout, "Closing %d fd\n", *fd);

    if (close(*fd) == -1) {
        fprintf(stderr, "%d fd failed to close\n", *fd);
        return -1;
    }
    *fd = -1;

    return 0;
}

static int xioctl(int fh, int request, void *arg)
{
    int r, retry = 0;

    do {
        r = ioctl(fh, request, arg);
        if (r == -1 && errno == EINTR) {
            ++retry;
            if (retry < 5)
                continue;
            else
                break;
        }
    } while (0);

    return r;
}

static int stop_capturing(v4l2_capture_t *dev)
{
    enum v4l2_buf_type type;
    int ret = 0;

    fprintf(stdout, "Stop v4l2 capturing\n");

    switch (dev->io) {
    case IO_METHOD_MMAP:
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(dev->fd, VIDIOC_STREAMOFF, &type) == -1) {
            fprintf(stderr, "VIDIOC_STREAMOFF failed\n");
            ret = -1;
        }
        break;

    default:
        fprintf(stderr, "%d io method failed\n", dev->io);
        ret = -1;
        break;
    }

    return ret;
}

static int start_streaming(v4l2_capture_t *dev)
{
    int ret = 0;
    unsigned int i;
    enum v4l2_buf_type type;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        for (i = 0; i < dev->buf_count; ++i) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (xioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
                fprintf(stderr, "VIDIOC_QBUF ioctl failed\n");
                ret = -1;
            }
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(dev->fd, VIDIOC_STREAMON, &type) == -1) {
            fprintf(stderr, "VIDIOC_STREAMON ioctl failed\n");
            ret = -1;
        }
        break;
    }

    return ret;
}

static int process_image(v4l2_capture_t *dev, const void *p,
    int size, unsigned int count)
{
    char file_name[200];
    FILE *fp;

    memset(file_name, 0, sizeof(file_name));

    if (dev->file_name == NULL) {
        fprintf(stderr, "Output file name is null. ret -1\n");
        return -1;
    }

    printf("Writing buffer into %s_%d file\n", dev->file_name, count);

    switch (dev->pix_fmt) {
    case V4L2_PIX_FMT_JPEG:
        sprintf(file_name, "%s_%d.jpg", dev->file_name, count);
        break;

    case V4L2_PIX_FMT_YUYV:
        sprintf(file_name, "%s_%d.yuv", dev->file_name, count);
        break;

    default:
        sprintf(file_name, "%s_%d.data", dev->file_name, count);
        break;
    }

    fp = fopen(file_name, "wb");
    if (fp == NULL) {
        fprintf(stderr, "%s failed to open. Error %s [%d]\n",
                file_name, strerror(errno), errno);
        return -1;
    }

    fwrite(p, size, 1, fp);
    fflush(stderr);
    fflush(fp);
    fclose(fp);

    return 0;
}


static int read_frame(v4l2_capture_t *dev, unsigned int count)
{
    struct v4l2_buffer buf;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(dev->fd, VIDIOC_DQBUF, &buf) == -1) {
            switch (errno) {
            case EAGAIN:
                return 0;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                return -1;
            }
        }

        assert(buf.index < dev->buf_count);
        if (process_image(dev, dev->buffer[buf.index].start,
            buf.bytesused, count) == -1)
            return -1;

        if (xioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
            fprintf(stderr, "VIDIOC_QBUF ioctl failed\n");
            return -1;
        }
        break;
    }

    return 0;
}

static int start_capturing(v4l2_capture_t *dev)
{
    fd_set fds;
    struct timeval tv;
    int ret;
    unsigned int count = 1;

    while (count <= dev->stream_count) {
        for (;;) {
            FD_ZERO(&fds);
            FD_SET(dev->fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            ret = select(dev->fd + 1, &fds, NULL, NULL, &tv);
            if (ret == -1) {
                if (errno == EINTR)
                    continue;
                fprintf(stderr, "Select error. ret -1\n");
                return -1;
            }
            if (ret == 0) {
                fprintf(stderr, "select timeout\n");
                exit(EXIT_FAILURE);
            }
            if (read_frame(dev, count) == 0) {
                fprintf(stderr, "Video capture is done\n");
                break;
            } else {
                fprintf(stderr, "Read frame err ret -1\n");
                return -1;
            }
        }
        ++count;
    }

    return 0;
}

static int init_mmap(v4l2_capture_t *dev)
{
    int n_buffer;
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof(struct v4l2_requestbuffers));

    req.count  = BUF_SIZE;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(dev->fd, VIDIOC_REQBUFS, &req) == -1) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n",
                    dev->dev_name);
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr, "VIDIOC_REQBUFS is failed\n");
            return -1;
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n",
                dev->dev_name);
        return -1;
    }

    dev->buffer = calloc(req.count, sizeof(*dev->buffer));
    if (!dev->buffer) {
        fprintf(stderr, "!!! Out of memory !!!\n");
        return -1;
    }

    for (n_buffer = 0; n_buffer < (int)req.count; ++n_buffer) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffer;

        if (xioctl(dev->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            fprintf(stderr, "VIDIOC_QUERYBUF is failed\n");
            return -1;
        }
        dev->buffer[n_buffer].length = buf.length;
        dev->buffer[n_buffer].start =
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
             dev->fd, buf.m.offset);

        if (dev->buffer[n_buffer].start == MAP_FAILED) {
            fprintf(stderr, "Memory map failed\n");
            return -1;
        }
    }
    dev->buf_count = n_buffer;

    return 0;
}

int uninit_video_device(v4l2_capture_t *dev)
{
    unsigned int i;
    int ret = 0;

    if (dev->buffer == NULL)
        return -1;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        for (i = 0; i < dev->buf_count; ++i)
             if (munmap(dev->buffer[i].start,
                 dev->buffer[i].length) == -1) {
                 fprintf(stderr, "Unmapping failed. Error %s (%d)\n",
                     strerror(errno), errno);
                 ret = -1;
             }
         break;

    default:
        fprintf(stderr, "%d io method not available\n", dev->io);
        ret = -1;
        break;
    }

    free(dev->buffer);

    return ret;
}

static int v4l2_set_parm(struct v4l2_streamparm *s_parm, int fd)
{
    if (s_parm->parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
        if (xioctl(fd, VIDIOC_S_PARM, s_parm) == -1) {
            fprintf(stderr, "V4L2 set parm failed. %s (%d)\n",
                    strerror(errno), errno);
            return -1;
        }
    }

    return 0;
}

static int v4l2_get_parm(struct v4l2_streamparm *stream_parm, int fd)
{
    stream_parm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_G_PARM, stream_parm) == -1) {
        fprintf(stderr, "V4L2 get parm failed. %s (%d)\n",
                strerror(errno), errno);
        return -1;
    }

    return 0;
}

static int v4l2_set_format(v4l2_capture_t *dev)
{
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(struct v4l2_format));

    if ((dev->width == 0) || (dev->height == 0) ||
        (dev->pix_fmt == 0)) {
        fprintf(stderr, "Invalid resoultion (W x H) %d x %d "
            "or pixel format - %d\n", dev->width, dev->height,
            dev->pix_fmt);
        return -1;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = dev->width;
    fmt.fmt.pix.height      = dev->height;
    fmt.fmt.pix.pixelformat = dev->pix_fmt;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    fprintf(stdout, "Setting up W x H (%d x %d)\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height);

    if (xioctl(dev->fd, VIDIOC_S_FMT, &fmt) == -1) {
        char sys_cmd[32];

        memset(sys_cmd, 0, sizeof(sys_cmd));
        sprintf(sys_cmd, "v4l2-ctl -d %s --list-formats-ext", dev->dev_name);
        fprintf(stderr, "\nSupport the following formats\n");
        if (system(sys_cmd) != 0)
            fprintf(stderr, "%s failed to execute on shell\n", sys_cmd);
        return -1;
    }

    return 0;
}

static int init_video_device(v4l2_capture_t *dev)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_streamparm stream_parm;
    int ret = 0;

    fprintf(stdout, "Initializing video device\n");

    memset(&cap, 0, sizeof(struct v4l2_capability));
    memset(&cropcap, 0, sizeof(struct v4l2_cropcap));
    memset(&crop, 0, sizeof(struct v4l2_cropcap));
    memset(&stream_parm, 0, sizeof(struct v4l2_streamparm));

    if (xioctl(dev->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        fprintf(stderr, "VIDIOC_QUERYCAP is failed. %s (%d).\n",
            strerror(errno), errno);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n", dev->dev_name);
        return -1;
    }

    switch (dev->io) {
    case IO_METHOD_MMAP:
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            fprintf(stderr, "%s does not support streaming i/o\n",
                dev->dev_name);
            return -1;
        }
        break;
    }

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(dev->fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        if (xioctl(dev->fd, VIDIOC_S_CROP, &crop) == -1) {
            switch (errno) {
            case EINVAL:
                /* Cropping not supported. */
                break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    } else {
        /* Errors ignored. */
    }

    if (v4l2_set_format(dev) == -1)
        return -1;

    if (v4l2_get_parm(&stream_parm, dev->fd) == -1)
        return -1;

    if (v4l2_set_parm(&stream_parm, dev->fd) == -1)
        return -1;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        ret = init_mmap(dev);
        break;
    default:
        printf("io changed to default mode IO_METHOD_MMAP\n");
        dev->io = IO_METHOD_MMAP;
        break;
    }

    return ret;
}

static int open_video_device(v4l2_capture_t *dev)
{
    int fd;
    struct stat st;

    fprintf(stdout, "Opening video device %s\n", dev->dev_name);

    if (stat(dev->dev_name, &st) == -1) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                dev->dev_name, errno, strerror(errno));
        return -1;
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev->dev_name);
        return -1;
    }

    fd = open(dev->dev_name, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev->dev_name,
                errno, strerror(errno));
        return -1;
    }
    dev->fd = fd;

    return 0;
}

int initialise_v4l2_capture(v4l2_capture_t *v4l2_dev)
{
    if (open_video_device(v4l2_dev) == -1)
        exit(1);

    if (init_video_device(v4l2_dev) == -1)
        return -1;

    if (start_streaming(v4l2_dev) == -1)
        return -1;

    if (start_capturing(v4l2_dev) == -1)
        return -1;

    return stop_capturing(v4l2_dev);
}
