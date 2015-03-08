// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/loader/smdh.h"
#include "video_core/color.h"
#include "video_core/utils.h"

std::vector<Math::Vec4<u8>> SMDH::DecodeIcon(bool big) {
    const u8* icon_data;
    int size;
    if (big) {
        icon_data = big_icon.data();
        size = 48;
    } else { // small
        icon_data = small_icon.data();
        size = 24;
    }
    std::vector<Math::Vec4<u8>> pixels(size * size);
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            int offset = VideoCore::GetMortonOffset(x, y, 2, size);
            pixels[size * y + x] = Color::DecodeRGB565(icon_data + offset);
        }
    }
    return pixels;
}

std::string SMDH::GetShortDescription(SMDH_ApplicationTitleLanguage language) {
    const SMDH_ApplicationTitle& app_title = app_titles[language];
    return Common::UTF16ToUTF8(app_title.short_description.data());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

////////////////////////////////////////////////////////////////////////////////////////////////////
// AppLoader_SMDH class

// TODO: Useful?
FileType IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1))
        return FileType::Error;

    if (MakeMagic('S', 'M', 'D', 'H') == magic)
        return FileType::SMDH;

    return FileType::Error;
}

std::unique_ptr<SMDH> Load(FileUtil::IOFile& file) {
    file.Seek(0, SEEK_SET);

    if (SMDH_Size != file.GetSize()) {
        LOG_ERROR(Loader, "SMDH file has invalid size.");
        return nullptr;
    }

    std::unique_ptr<SMDH> smdh = std::unique_ptr<SMDH>(new SMDH);
    auto ret = file.ReadBytes(static_cast<void*>(&*smdh), SMDH_Size);
    if (SMDH_Size != ret) {
        LOG_ERROR(Loader, "TODO %d", ret);
        return nullptr;
    }

    ASSERT(MakeMagic('S', 'M', 'D', 'H') == smdh->magic);

    return smdh;
}

} // namespace Loader
