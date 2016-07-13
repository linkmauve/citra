// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/memory.h"

#include "core/hle/result.h"
#include "core/hle/service/cam/impl.h"

namespace Service {
namespace CAM {

ResultCode StartCapture(u8 port) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode StopCapture(u8 port) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode SetReceiving(VAddr dest, u8 port, u32 image_size, u16 trans_unit) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode Activate(u8 cam_select) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode SetSize(u8 cam_select, u8 size, u8 context) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode SetOutputFormat(u8 cam_select, u8 output_format, u8 context) {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode DriverInitialize() {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

ResultCode DriverFinalize() {
    LOG_ERROR(Service_CAM, "Unimplemented!");
    return UnimplementedFunction(ErrorModule::CAM);
}

} // namespace CAM

} // namespace Service
