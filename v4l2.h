#ifndef __V4L2CAPTURE_H__
#define __V4L2CAPTURE_H__

#define BUF_SIZE                 (4)
#define STREAM_COUNT             (1)
#define IO_METHOD     IO_METHOD_MMAP
#define DEFAULT_VDEV   "/dev/video0"
#define V4L2_GET_ANALOG_GAIN  (0x01)
#define V4L2_SET_ANALOG_GAIN  (0x02)

typedef enum io_method_s {
    IO_METHOD_MMAP    = 0,
    /* IO_METHOD_USERPTR has to be implemented if necessory */
} io_method_e;

typedef struct v4l2_buffer_s {
    void   *start;
    size_t length;
} v4l2_buffer_t;

typedef struct v4l2_capture_s {
    int  fd;
    char *dev_name;
    char *file_name;

    uint32_t     pix_fmt;
    unsigned int buf_count;
    unsigned int stream_count;
    unsigned int width;
    unsigned int height;

    io_method_e io;
    v4l2_buffer_t *buffer;
} v4l2_capture_t;

int initialise_v4l2_capture(v4l2_capture_t *v4l2_dev);
int close_video_device(int *fd);
int uninit_video_device(v4l2_capture_t *dev);

#endif /* __V4L2CAPTURE_H__ */
