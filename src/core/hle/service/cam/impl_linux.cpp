// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/memory.h"

#include "core/hle/result.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/impl.h"

namespace Service {
namespace CAM {

// TODO(Link Mauve): make the video device configurable.
static const char *video_device = "/dev/video0";
static const unsigned NUM_BUFFERS = 2;

static int fd;
static enum v4l2_buf_type format_type;
static struct v4l2_format format;
static u32 width;
static u32 height;
static bool crop_image_horizontally;

static struct {
    char *memory;
    size_t length;
} mapped_buffers[NUM_BUFFERS];

ResultCode StartCapture(u8 port) {
    if (ioctl(fd, VIDIOC_STREAMON, &format_type) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_STREAMON: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    return RESULT_SUCCESS;
}

ResultCode StopCapture(u8 port) {
    if (ioctl(fd, VIDIOC_STREAMOFF, &format_type) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_STREAMOFF: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    return RESULT_SUCCESS;
}

ResultCode SetReceiving(VAddr dest, u8 port, u32 image_size, u16 trans_unit) {
    // XXX: hack stereoscopy somehow.
    if (!(port & 1))
        return ResultCode(ErrorDescription::NotImplemented, ErrorModule::CAM,
                          ErrorSummary::NotSupported, ErrorLevel::Info);

    struct v4l2_buffer buf;

    std::memset(&buf, '\0', sizeof(buf));
    buf.type = format_type;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue a buffer, at this point we can safely read from it.
    // TODO(Link Mauve): this ioctl is blocking until a buffer is ready, maybe we don’t want that?
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_DQBUF: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    ASSERT(buf.flags & V4L2_BUF_FLAG_DONE);

    unsigned index = buf.index;
    ASSERT(index < NUM_BUFFERS);
    const char *memory = mapped_buffers[index].memory;

    // Not every capture device supports the size requested.
    if (crop_image_horizontally) {
        // There are always two bytes per pixel, both for YUYV and for RGB565.
        const unsigned bytes_per_pixel = 2;
        for (int y = 0; y < height; ++y)
            Memory::WriteBlock(dest + (width * bytes_per_pixel * y),
                               static_cast<const char*>(memory) + (format.fmt.pix.bytesperline * y),
                               width * bytes_per_pixel);
    } else {
        Memory::WriteBlock(dest, memory, image_size);
    }

    // Enqueue back the buffer we just copied, so it will be filled again.
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_QBUF: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    return RESULT_SUCCESS;
}

ResultCode Activate(u8 cam_select) {
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    // TODO(Link Mauve): apparently used to deactivate them, according to 3ds-examples.
    if (cam_select == 0) {
        return ResultCode(ErrorDescription::NotImplemented, ErrorModule::CAM,
                          ErrorSummary::NothingHappened, ErrorLevel::Info);
    }

    // We are using the mmap interface instead of the user-pointer one because the user could
    // change the target address at any frame.
    std::memset(&req, '\0', sizeof(req));
    req.type = format_type;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = NUM_BUFFERS;

    // Allocate our NUM_BUFFERS buffers.
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_REQBUFS: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    if (req.count < NUM_BUFFERS) {
        LOG_CRITICAL(Service_CAM, "wrong number of buffers!");
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    for (unsigned i = 0; i < NUM_BUFFERS; ++i) {
        std::memset(&buf, '\0', sizeof(buf));
        buf.type = format_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        // Query the characteristics of the newly-allocated buffer.
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            LOG_ERROR(Service_CAM, "VIDIOC_QUERYBUF: %s", std::strerror(errno));
            // TODO(Link Mauve): find the proper error code.
            return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                              ErrorSummary::Internal, ErrorLevel::Fatal);
        }

        // Map the buffer so we can copy from this address whenever we get more data.
        mapped_buffers[i].memory = static_cast<char*>(mmap(nullptr, buf.length, PROT_READ,
                                                           MAP_SHARED, fd, buf.m.offset));
        mapped_buffers[i].length = buf.length;

        // Enqueue the buffer to let the driver fill it whenever it gets an image.
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            LOG_ERROR(Service_CAM, "VIDIOC_QBUF: %s", std::strerror(errno));
            // TODO(Link Mauve): find the proper error code.
            return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                              ErrorSummary::Internal, ErrorLevel::Fatal);
        }
    }

    return RESULT_SUCCESS;
}

ResultCode SetSize(u8 cam_select, u8 size, u8 context) {
    switch (static_cast<Size>(size)) {
    case Size::VGA:
        width = 640;
        height = 480;
        break;
    case Size::QVGA:
        width = 320;
        height = 240;
        break;
    case Size::QQVGA:
        width = 160;
        height = 120;
        break;
    case Size::CIF:
        width = 352;
        height = 288;
        break;
    case Size::QCIF:
        width = 176;
        height = 144;
        break;
    case Size::DS_LCD:
        width = 256;
        height = 192;
        break;
    case Size::DS_LCDx4:
        width = 512;
        height = 384;
        break;
    case Size::CTR_TOP_LCD:
        width = 400;
        height = 240;
        break;
    default:
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidSize, ErrorModule::CAM,
                          ErrorSummary::WrongArgument, ErrorLevel::Fatal);
    }

    format.fmt.pix.width = width;
    format.fmt.pix.height = height;

    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_S_FMT: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    if (format.fmt.pix.width > width) {
        LOG_WARNING(Service_CAM, "Got a larger format than requested (%d > %d), will crop",
                    format.fmt.pix.width, width);
        crop_image_horizontally = true;
    } else {
        crop_image_horizontally = false;
    }

    if (format.fmt.pix.height > height)
        LOG_WARNING(Service_CAM, "Got a higher format than requested (%d > %d), will crop",
                    format.fmt.pix.height, height);

    return RESULT_SUCCESS;
}

static inline const char * dump_format(uint32_t format, char out[4])
{
#if BYTE_ORDER == BIG_ENDIAN
    format = __builtin_bswap32(format);
#endif
    memcpy(out, &format, 4);
    return out;
}

ResultCode SetOutputFormat(u8 cam_select, u8 output_format, u8 context) {
    u32 v4l2_format;

    switch (static_cast<OutputFormat>(output_format)) {
    case OutputFormat::YUV422:
        v4l2_format = V4L2_PIX_FMT_YUYV;
        break;
    case OutputFormat::RGB565:
        v4l2_format = V4L2_PIX_FMT_RGB565;
        break;
    default:
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                          ErrorSummary::WrongArgument, ErrorLevel::Fatal);
    }

    format.fmt.pix.pixelformat = v4l2_format;

    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_S_FMT: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // TODO(Link Mauve): use y2r:u instead when the user asks for RGB565, since the vast majority
    // of capture devices won’t give us that kind of format.
    if (format.fmt.pix.pixelformat != v4l2_format) {
        char buf1[4], buf2[4];
        LOG_ERROR(Service_CAM, "Wrong pixel format, asked %.4s got %.4s!",
                  dump_format(v4l2_format, buf1), dump_format(format.fmt.pix.pixelformat, buf2));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::NotImplemented, ErrorModule::CAM,
                          ErrorSummary::NotSupported, ErrorLevel::Permanent);
    }

    return RESULT_SUCCESS;
}

ResultCode DriverInitialize() {
    struct v4l2_capability cap;

    // Open the chosen v4l2 device.
    fd = open(video_device, O_RDWR);
    if (fd < 0) {
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::NotFound, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // Query the capabilities of that device.
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_QUERYCAP: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // We only support single-planar video capture devices currently, which represent the vast
    // majority of camera devices.
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        format_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else {
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // We are using the streaming interface.
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG_CRITICAL(Service_CAM, "Not a streaming device!");
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // Retrieve the current format, which will be modified afterwards.
    std::memset(&format, '\0', sizeof(format));
    format.type = format_type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1) {
        LOG_ERROR(Service_CAM, "VIDIOC_G_FMT: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    // TODO(Link Mauve): find the default format on a 3DS and set it here.

    return RESULT_SUCCESS;
}

ResultCode DriverFinalize() {
    // TODO(Link Mauve): maybe this should be done in Activate(0) instead.
    for (unsigned i = 0; i < NUM_BUFFERS; ++i) {
        if (mapped_buffers[i].memory)
            munmap(mapped_buffers[i].memory, mapped_buffers[i].length);
        mapped_buffers[i].memory = nullptr;
    }

    // Exits the API.
    if (close(fd) == -1) {
        LOG_ERROR(Service_CAM, "close: %s", std::strerror(errno));
        // TODO(Link Mauve): find the proper error code.
        return ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::CAM,
                          ErrorSummary::Internal, ErrorLevel::Fatal);
    }

    return RESULT_SUCCESS;
}

} // namespace CAM

} // namespace Service
