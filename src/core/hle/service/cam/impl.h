// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/result.h"
#include "core/memory.h"

namespace Service {
namespace CAM {

ResultCode StartCapture(u8 port);
ResultCode StopCapture(u8 port);
ResultCode SetReceiving(VAddr dest, u8 port, u32 image_size, u16 trans_unit);
ResultCode Activate(u8 cam_select);
ResultCode SetSize(u8 cam_select, u8 size, u8 context);
ResultCode SetOutputFormat(u8 cam_select, u8 output_format, u8 context);
ResultCode DriverInitialize();
ResultCode DriverFinalize();

} // namespace CAM

} // namespace Service
