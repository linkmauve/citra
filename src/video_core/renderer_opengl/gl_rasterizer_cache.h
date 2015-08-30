// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>

#include "video_core/pica.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"

namespace RendererGL {

struct CachedSurface {
    enum class ColorFormat {
        // First 5 formats are shared between textures and color buffers
        RGBA8        =  0,
        RGB8         =  1,
        RGB5A1       =  2,
        RGB565       =  3,
        RGBA4        =  4,

        // Texture-only formats
        IA8          =  5,
        RG8          =  6,
        I8           =  7,
        A8           =  8,
        IA4          =  9,
        I4           = 10,
        A4           = 11,
        ETC1         = 12,
        ETC1A4       = 13,

        // Depth buffer-only formats
        D16          = 14,
        D24          = 15,
        D24S8        = 16,

        Invalid      = 255,
    };

    static ColorFormat ColorFormatFromTextureFormat(Pica::Regs::TextureFormat format) {
        return ((unsigned int)format < 14) ? (ColorFormat)format : ColorFormat::Invalid;
    }

    static ColorFormat ColorFormatFromColorFormat(Pica::Regs::ColorFormat format) {
        return ((unsigned int)format < 5) ? (ColorFormat)format : ColorFormat::Invalid;
    }

    static ColorFormat ColorFormatFromDepthFormat(Pica::Regs::DepthFormat format) {
        return ((unsigned int)format < 3) ? (ColorFormat)((unsigned int)format + 14) : ColorFormat::Invalid;
    }

    enum class TilingFormat {
        Linear = 0,
        Block8x8 = 1,
        ClearPending = 2,
    };

    PAddr addr;
    u32 size;
    u64 hash;

    OGLTexture texture;
    u32 width;
    u32 height;

    TilingFormat tiling_format;
    ColorFormat color_format;
    u32 clear_color;
    bool dirty;
};

class SurfaceCache : NonCopyable {
public:
    /// Loads a texture from 3DS memory to OpenGL and caches it (if not already cached)
    CachedSurface* GetSurface(OpenGLState &state, unsigned texture_unit, const CachedSurface& params);

    void LoadAndBindTexture(OpenGLState &state, unsigned texture_unit, const Pica::Regs::FullTextureConfig& config);
    void LoadAndBindFramebuffer(OpenGLState& state, unsigned color_tex_unit, unsigned depth_tex_unit, const Pica::Regs::FramebufferConfig& config);

    void InvalidateSurface(CachedSurface* surface);
    void FlushSurface(OpenGLState& state, unsigned int texture_unit, CachedSurface* surface);

    /// Invalidate any cached resource that overlaps the region
    void InvalidateInRange(PAddr addr, u32 size, bool ignore_hash = false);
    /// Write any cached resources overlapping the region back to memory
    void FlushInRange(OpenGLState& state, PAddr addr, u32 size);

    /// Flush all cached OpenGL resources tracked by this cache manager
    void FlushAll();

private:
    std::map<PAddr, std::unique_ptr<CachedSurface>> texture_cache;
};

}
