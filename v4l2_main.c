#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <linux/videodev2.h>
#include <getopt.h>

#include "v4l2.h"

static void initialise_v4l2data(v4l2_capture_t *v4l2_data)
{
    v4l2_data->io           = IO_METHOD;
    v4l2_data->stream_count = STREAM_COUNT;
    v4l2_data->file_name    = NULL;
    v4l2_data->dev_name     = NULL;

    return;
}

static void usage(char **argv)
{
    fprintf(stdout, "Usage: %s [options]\n\n"
            "-h | --help          Print this message\n"
            "-d | --device name   Video device name\n"
            "                     Eg: -d /dev/video0\n"
            "-o | --output        Output file name\n"
            "                     Eg: -o output\n"
            "-m | --mmap          Use memory mapped buffers [default]\n"
            "                     0 - IO_METHOD_MMAP\n"
            "                     1 - IO_METHOD_USERPTR\n"
            "-f | --format        Force format\n"
            "                     1 - V4L2_PIX_FMT_JPEG (MJPEG)\n"
            "                     2 - V4L2_PIX_FMT_YUYV (YUYV/YUYV 4:2:2)\n"
            "                     3 - V4L2_PIX_FMT_SRGGB10 (RAW RGGB10)\n"
            "                     4 - V4L2_PIX_FMT_SGBRG10 (RAW GBRG10)\n"
            "-c | --count         Number of frames to grab\n"
            "-s | --resoultion    Use specified input size (W x H)\n"
            "                     Eg: 640x480, 1280x720, 1920x1080\n"
            "                         3840x2160, 4056x3040\n"
            "",
            argv[0]);
}

int main(int argc, char **argv)
{
    v4l2_capture_t v4l2_data;
    int i = 1;
    int option, width, height;
    char *separater = NULL;
    char *string_size = NULL;

    memset(&v4l2_data, 0, sizeof(v4l2_data));

    initialise_v4l2data(&v4l2_data);

    while ((option = getopt(argc, argv, "h:d:f:o:c:s:")) != -1) {
        switch (option) {
            case 'h':
                usage(argv);
                break;
            case 'd':
                v4l2_data.dev_name = optarg;
                break;
            case 'o':
                v4l2_data.file_name = optarg;
                break;
            case 'c':
                v4l2_data.stream_count = atoi(optarg);
                break;
            case 'i':
                switch (atoi(optarg)) {
                    case 0:
                        v4l2_data.io = IO_METHOD_MMAP;
                        break;
                    default:
                        v4l2_data.io = IO_METHOD_MMAP;
                        break;
                }
                break;
            case 'f':
                switch (atoi(optarg)) {
                    case 1:
                        v4l2_data.pix_fmt = V4L2_PIX_FMT_JPEG;
                        break;
                    case 2:
                        v4l2_data.pix_fmt = V4L2_PIX_FMT_YUYV;
                        break;
                    case 3:
                        v4l2_data.pix_fmt = V4L2_PIX_FMT_SRGGB10;
                        break;
                    case 4:
                        v4l2_data.pix_fmt = V4L2_PIX_FMT_SGBRG10;
                        break;
                    default:
                         printf("%d is invalid pixel format. "
                                "Please refer below\n", atoi(optarg));
                         usage(argv);
                         return -1;
                }
                break;
            case 's':
                string_size = strdup(optarg);
                width = strtoul(string_size, &separater, 10);
                if (*separater != 'x') {
                    printf("Error in size use -s widthxheight \n");
                    exit(1);
                } else {
                    ++separater;
                    height = strtoul(separater, &separater, 10);
                    if (*separater != 0)
                        printf("Fail to parse the height\n");
                    if ((width <= 0) && (height <= 0)) {
                        printf("Invalid Width (%d) and height (%d)\n",
                               width, height);
                        exit(1);
                    }
                    v4l2_data.width  = width;
                    v4l2_data.height = height;
                }
                break;
            default:
               printf("%c is invalid option\n", option);
               usage(argv);
               return 1;
        }
        ++i;
    }

    if (v4l2_data.pix_fmt <= 0) {
        fprintf(stderr, "Please provide pixel format\n");
        usage(argv);
        return -1;
    }

    if (v4l2_data.width <= 0 || v4l2_data.height <= 0) {
        fprintf(stderr, "Resolution size W x H (%d x %d) is invalid\n",
                v4l2_data.width, v4l2_data.height);
        usage(argv);
        return -1;
    }

    if (v4l2_data.file_name == NULL) {
        fprintf(stderr, "Please provide output file name to store data\n");
        usage(argv);
        return -1;
    }

    if (v4l2_data.dev_name == NULL) {
        fprintf(stdout, "Default video device assigned for capturing videos\n");
        v4l2_data.dev_name = DEFAULT_VDEV;
    }

    initialise_v4l2_capture(&v4l2_data);
    close_video_device(&v4l2_data.fd);
    uninit_video_device(&v4l2_data);

    return 0;
}
