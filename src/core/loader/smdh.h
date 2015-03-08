// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "core/loader/loader.h"
#include "video_core/math.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// SMDH header (Note: "SMDH" appears to be a publically unknown acronym)

struct SMDH_ApplicationTitle {
    std::array<char16_t, 0x40> short_description;
    std::array<char16_t, 0x80> long_description;
    std::array<char16_t, 0x40> publisher;
};

enum SMDH_ApplicationTitleLanguage : u8 {
    Japanese = 0,
    English,
    French,
    German,
    Italian,
    Spanish,
    SimplifiedChinese,
    Korean,
    Dutch,
    Portuguese,
    Russian,
    TraditionalChinese,
};

struct SMDH_GameRatings {
    u8 cero; // Japan
    u8 esrb; // USA
    u8 reserved_1;
    u8 usk; // German
    u8 pegi_gen; // Europe
    u8 reserved_2;
    u8 pegi_prt; //  Portugal
    u8 pegi_bbfc; // England
    u8 cob; // Australia
    u8 grb; // South Korea
    u8 cgsrr; // China
    u8 reserved_3[5];
};

enum SMDH_RegionLockout : u32 {
    japan = 1,
    north_america = 2,
    europe = 4,
    australia = 8,
    china = 16,
    korea = 32,
    taiwan = 64,
};

struct SMDH_MatchMakerId {
    std::array<u8, 4> id;
    std::array<u8, 8> bit_id;
};
static_assert(12 == sizeof(SMDH_MatchMakerId), "SMDH_MatchMakerId struct should always be 12 bytes long.");

enum SMDH_Flags : u32 {
    visibility = 1,
    auto_boot = 2,
    allow_3d = 4,
    require_accepting_eula = 8,
    autosave_on_exit = 16,
    use_extended_banner = 32,
    region_game_rating_required = 64,
    use_save_data = 128,
    record_application_usage = 256,

    disable_sd_savedata_backups = 1024,
};

struct SMDH_EulaVersion {
    u8 minor;
    u8 major;
};
static_assert(2 == sizeof(SMDH_EulaVersion), "SMDH_EulaVersion struct should always be 2 bytes long.");

struct SMDH_ApplicationSettings {
    SMDH_GameRatings game_ratings;
    SMDH_RegionLockout region_lockout;
    SMDH_MatchMakerId match_maker_id;
    SMDH_Flags flags;
    SMDH_EulaVersion eula_version;
    u16 reserved;
    u32 animation_default_frame;
    u32 streetpass_id;
};
static_assert(0x30 == sizeof(SMDH_ApplicationSettings), "SMDH_ApplicationSettings struct should always be 0x30 bytes long.");

struct SMDH {
    u32 magic;
    u16 version;
    u16 reserved_1;
    std::array<SMDH_ApplicationTitle, 16> app_titles;
    SMDH_ApplicationSettings app_settings;
    u8 reserved_2[8];
    std::array<u8, 0x480> small_icon;
    std::array<u8, 0x1200> big_icon;

    /** TODO */
    std::vector<Math::Vec4<u8>> DecodeIcon(bool big);

    /** TODO */
    std::string GetShortDescription(SMDH_ApplicationTitleLanguage language);
};

static const size_t SMDH_Size = 0x36c0;

static_assert(SMDH_Size == sizeof(SMDH), "SMDH struct should always be 0x36c0 bytes long.");
static_assert(std::is_pod<SMDH>::value, "SMDH struct isn’t POD.");
static_assert(std::is_trivial<SMDH>::value, "SMDH struct isn’t trivial.");
static_assert(std::is_standard_layout<SMDH>::value, "SMDH struct isn’t standard layout.");

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/** TODO */
FileType IdentifyType(FileUtil::IOFile& file);

/** TODO */
std::unique_ptr<SMDH> Load(FileUtil::IOFile& file);

} // namespace Loader
