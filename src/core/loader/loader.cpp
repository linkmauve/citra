// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include "common/make_unique.h"

#include "core/file_sys/archive_romfs.h"
#include "core/loader/3dsx.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"
#include "core/hle/service/fs/archive.h"
#include "core/mem_map.h"

#ifdef HAVE_PNG
#include "core/loader/smdh.h"
#include <png.h>
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Loader {

/**
 * Identifies the type of a bootable file
 * @param file open file
 * @return FileType of file
 */
static FileType IdentifyFile(FileUtil::IOFile& file) {
    FileType type;

#define CHECK_TYPE(loader) \
    type = AppLoader_##loader::IdentifyType(file); \
    if (FileType::Error != type) \
        return type;

    CHECK_TYPE(THREEDSX)
    CHECK_TYPE(ELF)
    CHECK_TYPE(NCCH)

#undef CHECK_TYPE

    return FileType::Unknown;
}

/**
 * Guess the type of a bootable file from its extension
 * @param filename String filename of bootable file
 * @return FileType of file
 */
static FileType GuessFromFilename(const std::string& filename) {
    if (filename.size() == 0) {
        LOG_ERROR(Loader, "invalid filename %s", filename.c_str());
        return FileType::Error;
    }

    size_t extension_loc = filename.find_last_of('.');
    if (extension_loc == std::string::npos)
        return FileType::Unknown;
    std::string extension = Common::ToLower(filename.substr(extension_loc));

    if (extension == ".elf")
        return FileType::ELF;
    else if (extension == ".axf")
        return FileType::ELF;
    else if (extension == ".cxi")
        return FileType::CXI;
    else if (extension == ".cci")
        return FileType::CCI;
    else if (extension == ".bin")
        return FileType::BIN;
    else if (extension == ".3ds")
        return FileType::CCI;
    else if (extension == ".3dsx")
        return FileType::THREEDSX;
    return FileType::Unknown;
}

static const char* GetFileTypeString(FileType type) {
    switch (type) {
    case FileType::CCI:
        return "NCSD";
    case FileType::CXI:
        return "NCCH";
    case FileType::ELF:
        return "ELF";
    case FileType::THREEDSX:
        return "3DSX";
    case FileType::BIN:
        return "raw";
    case FileType::Error:
    case FileType::Unknown:
        break;
    }

    return "unknown";
}

static void WriteIconPNG(const std::string& filename, int width, int height, u8* data) {
#ifndef HAVE_PNG
    return;
#else
    if (!data)
        return;

    // Write data to file
    static int dump_index = 0;
    u32 row_stride = width * 4;

    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;

    // Open file for writing (binary mode)
    FileUtil::IOFile fp(filename, "wb");

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png_ptr == nullptr) {
        LOG_ERROR(Debug_GPU, "Could not allocate write struct\n");
        goto finalise;

    }

    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        LOG_ERROR(Debug_GPU, "Could not allocate info struct\n");
        goto finalise;
    }

    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        LOG_ERROR(Debug_GPU, "Error during png creation\n");
        goto finalise;
    }

    png_init_io(png_ptr, fp.GetHandle());

    // Write header (8 bit color depth)
    png_set_IHDR(png_ptr, info_ptr, width, height,
        8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_text text[3];
    text[0].compression = PNG_TEXT_COMPRESSION_NONE;
    text[0].key = "Title";
    text[0].text = "SMDH Icon";

    text[1].compression = PNG_TEXT_COMPRESSION_NONE;
    text[1].key = "Thumb::URI";
    text[1].text = "file:///home/linkmauve/test.cxi";

    text[2].compression = PNG_TEXT_COMPRESSION_NONE;
    text[2].key = "Thumb::MTime";
    text[2].text = "1430857176";

    text[3].compression = PNG_TEXT_COMPRESSION_NONE;
    text[3].key = "Software";
    text[3].text = "Citra";
    png_set_text(png_ptr, info_ptr, text, 4);

    png_write_info(png_ptr, info_ptr);

    // Write image data
    for (unsigned y = 0; y < height; ++y)
    {
        u8* row_ptr = data + y * row_stride;
        u8* ptr = row_ptr;
        png_write_row(png_ptr, row_ptr);
    }

    // End write
    png_write_end(png_ptr, nullptr);

finalise:
    if (info_ptr != nullptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != nullptr) png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
#endif
}

std::string urlencode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << '%' << std::setw(2) << int((unsigned char) c);
    }

    return escaped.str();
}

ResultStatus LoadFile(const std::string& filename) {
    std::unique_ptr<FileUtil::IOFile> file(new FileUtil::IOFile(filename, "rb"));
    if (!file->IsOpen()) {
        LOG_ERROR(Loader, "Failed to load file %s", filename.c_str());
        return ResultStatus::Error;
    }

    FileType type = IdentifyFile(*file);
    FileType filename_type = GuessFromFilename(filename);

    if (type != filename_type) {
        LOG_WARNING(Loader, "File %s has a different type than its extension.", filename.c_str());
        if (FileType::Unknown == type)
            type = filename_type;
    }

    LOG_INFO(Loader, "Loading file %s as %s...", filename.c_str(), GetFileTypeString(type));

    switch (type) {

    //3DSX file format...
    case FileType::THREEDSX:
        return AppLoader_THREEDSX(std::move(file)).Load();

    // Standard ELF file format...
    case FileType::ELF:
        return AppLoader_ELF(std::move(file)).Load();

    // NCCH/NCSD container formats...
    case FileType::CXI:
    case FileType::CCI:
    {
        AppLoader_NCCH app_loader(std::move(file));

        // Load application and RomFS
        if (ResultStatus::Success == app_loader.Load()) {
            Kernel::g_program_id = app_loader.GetProgramId();
            Service::FS::RegisterArchiveType(Common::make_unique<FileSys::ArchiveFactory_RomFS>(app_loader), Service::FS::ArchiveIdCode::RomFS);

#ifdef HAVE_PNG
            // TODO: move that where it actually makes sense.
            std::vector<u8> icon_data;
            if (ResultStatus::Success == app_loader.ReadIcon(icon_data)) {
                const char* xdg_cache_home = getenv("XDG_CACHE_HOME");
                std::string cache_home;
                if (xdg_cache_home && xdg_cache_home[0])
                    cache_home = xdg_cache_home;
                else
                    cache_home = std::string(getenv("HOME")) + "/" ".cache";
                std::string directory = cache_home + "/" "thumbnails" "/" "normal" "/";

                std::string urlencoded = urlencode("file://" + filename);
                printf("'%s'\n", urlencoded.c_str());

                std::string icon_filename = directory + "56466e00fcfcc6555b885c86360955ff" ".png";

                SMDH* smdh = reinterpret_cast<SMDH*>(icon_data.data());
                auto big_icon = smdh->DecodeIcon(true);
                WriteIconPNG(icon_filename, 48, 48, reinterpret_cast<u8*>(big_icon.data()));
            }
#endif

            return ResultStatus::Success;
        }
        break;
    }

    // Raw BIN file format...
    case FileType::BIN:
    {
        size_t size = (size_t)file->GetSize();
        if (file->ReadBytes(Memory::GetPointer(Memory::EXEFS_CODE_VADDR), size) != size)
            return ResultStatus::Error;

        Kernel::LoadExec(Memory::EXEFS_CODE_VADDR);
        return ResultStatus::Success;
    }

    // Error occurred durring IdentifyFile...
    case FileType::Error:

    // IdentifyFile could know identify file type...
    case FileType::Unknown:
    {
        LOG_CRITICAL(Loader, "File %s is of unknown type.", filename.c_str());
        return ResultStatus::ErrorInvalidFormat;
    }
    }
    return ResultStatus::Error;
}

} // namespace Loader
