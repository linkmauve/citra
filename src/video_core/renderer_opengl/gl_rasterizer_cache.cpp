// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hash.h"
#include "common/make_unique.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/vector_math.h"

#include "core/memory.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/utils.h"

namespace RendererGL {

static unsigned int GetFormatBpp(CachedSurface::ColorFormat format) {
    static const unsigned int bpp_table[] = {
        32, // RGBA8
        24, // RGB8
        16, // RGB5A1
        16, // RGB565
        16, // RGBA4
        16, // IA8
        16, // RG8
        8,  // I8
        8,  // A8
        8,  // IA4
        4,  // I4
        4,  // A4
        4,  // ETC1
        8,  // ETC1A4
        16, // D16
        0,
        24, // D24
        32, // D24S8
    };

    ASSERT((unsigned int)format < ARRAY_SIZE(bpp_table));
    return bpp_table[(unsigned int)format];
}

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;
};

static const FormatTuple fb_format_tuples[] = {
    {GL_RGBA8,   GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},   // RGBA8
    {GL_RGB8,    GL_BGR,  GL_UNSIGNED_BYTE},          // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565,  GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},   // RGB565
    {GL_RGBA4,   GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4}, // RGBA4
};

static const FormatTuple depth_format_tuples[] = {
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},    // D16
    {},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},      // D24
    {GL_DEPTH24_STENCIL8,  GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8}, // D24S8
};

MICROPROFILE_DEFINE(OpenGL_TextureUpload, "OpenGL", "Texture Upload", MP_RGB(128, 64, 192));

CachedSurface* SurfaceCache::GetSurface(OpenGLState& state, unsigned texture_unit, const CachedSurface& params) {
    using ColorFormat = CachedSurface::ColorFormat;

    auto it = texture_cache.find(params.addr);
    if (it != texture_cache.end()) {
        CachedSurface* surface = it->second.get();
        if (surface->addr == params.addr &&
            surface->width == params.width && surface->height == params.height &&
            surface->tiling_format == params.tiling_format &&
            surface->color_format == params.color_format)
        {
            state.texture_units[texture_unit].texture_2d = surface->texture.handle;
            state.Apply();
            //llog LOG_WARNING(HW_GPU, "Texture reused: %x%s", surface->addr, surface->dirty ? " (dirty)" : "");
            return surface;
        }
    }

    //llog LOG_WARNING(HW_GPU, "New texture: %x", params.addr);

    MICROPROFILE_SCOPE(OpenGL_TextureUpload);

    std::unique_ptr<CachedSurface> new_texture = Common::make_unique<CachedSurface>();

    u8* texture_src_data = Memory::GetPhysicalPointer(params.addr);
    new_texture->addr = params.addr;
    new_texture->size = params.width * params.height * GetFormatBpp(params.color_format) / 8;
    new_texture->hash = Common::ComputeHash64(texture_src_data, new_texture->size);

    new_texture->texture.Create();
    new_texture->width = params.width;
    new_texture->height = params.height;

    new_texture->tiling_format = params.tiling_format;
    new_texture->color_format = params.color_format;
    new_texture->clear_color = 0;
    new_texture->dirty = false;

    FlushInRange(state, texture_unit, new_texture->addr, new_texture->size);
    InvalidateInRange(new_texture->addr, new_texture->size, true);

    //llog LOG_WARNING(HW_GPU, "<---");

    state.texture_units[texture_unit].texture_2d = new_texture->texture.handle;
    state.Apply();
    glActiveTexture(GL_TEXTURE0 + texture_unit);

    if (params.tiling_format == CachedSurface::TilingFormat::Linear) {
        // Only a few LCD framebuffer formats can be linear, so we'll fast-path them using OpenGL's
        // built-in texture formats.

        ASSERT((unsigned int)params.color_format < ARRAY_SIZE(fb_format_tuples));
        const FormatTuple& tuple = fb_format_tuples[(unsigned int)params.color_format];
        glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, params.width, params.height, 0,
                tuple.format, tuple.type, texture_src_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else if (params.tiling_format == CachedSurface::TilingFormat::Block8x8) {
        if (params.color_format != ColorFormat::D16 &&
            params.color_format != ColorFormat::D24 &&
            params.color_format != ColorFormat::D24S8) {

            std::unique_ptr<Math::Vec4<u8>[]> tex_buffer(new Math::Vec4<u8>[params.width * params.height]);

            Pica::DebugUtils::TextureInfo tex_info;
            tex_info.width = params.width;
            tex_info.height = params.height;
            tex_info.stride = params.width * GetFormatBpp(params.color_format) / 8;
            tex_info.format = (Pica::Regs::TextureFormat)params.color_format;
            tex_info.physical_address = params.addr;

            for (int y = 0; y < params.height; ++y) {
                for (int x = 0; x < params.width; ++x) {
                    tex_buffer[x + params.width * y] = Pica::DebugUtils::LookupTexture(texture_src_data, x, params.height - 1 - y, tex_info);
                }
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.width, params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_buffer.get());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        } else {
            // Depth/Stencil formats need special treatment since they aren't sampleable using LookupTexture and can't use RGBA format

            u32 bytes_per_pixel = GetFormatBpp(params.color_format) / 8;

            // OpenGL needs 4 bpp alignment for D24
            u32 gl_bpp = bytes_per_pixel == 3 ? 4 : bytes_per_pixel;

            std::unique_ptr<u8[]> temp_fb_depth_buffer(new u8[params.width * params.height * gl_bpp]);

            u8* temp_fb_depth_data = bytes_per_pixel == 3 ? (temp_fb_depth_buffer.get() + 1) : temp_fb_depth_buffer.get();

            if (params.color_format == ColorFormat::D24S8) {
                for (int y = 0; y < params.height; ++y) {
                    for (int x = 0; x < params.width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, 4) + coarse_y * params.width * 4;
                        u32 gl_pixel_index = (x + (params.height - 1 - y) * params.width);

                        u8* pixel = texture_src_data + dst_offset;
                        u32 depth_stencil = *(u32*)pixel;
                        ((u32*)temp_fb_depth_data)[gl_pixel_index] = (depth_stencil << 8) | (depth_stencil >> 24);
                    }
                }
            } else {
                for (int y = 0; y < params.height; ++y) {
                    for (int x = 0; x < params.width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * params.width * bytes_per_pixel;
                        u32 gl_pixel_index = (x + (params.height - 1 - y) * params.width) * gl_bpp;

                        u8* pixel = texture_src_data + dst_offset;
                        memcpy(&temp_fb_depth_data[gl_pixel_index], pixel, bytes_per_pixel);
                    }
                }
            }

            unsigned int tuple_idx = (unsigned int)params.color_format - 14;
            ASSERT(tuple_idx < ARRAY_SIZE(depth_format_tuples));
            const FormatTuple& tuple = depth_format_tuples[tuple_idx];
            glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, params.width, params.height, 0,
                tuple.format, tuple.type, temp_fb_depth_buffer.get());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    } else if (params.tiling_format == CachedSurface::TilingFormat::ClearPending) {
        // TODO
    }

    auto result = texture_cache.emplace(params.addr, std::move(new_texture));
    ASSERT(result.second == true);
    return result.first->second.get();
}

CachedSurface* SurfaceCache::GetSurfaceRect(OpenGLState& state, unsigned texture_unit, const CachedSurface& params, MathUtil::Rectangle<int>& out_rect) {
    size_t params_size = params.width * params.height * GetFormatBpp(params.color_format) / 8;
    //auto cache_upper_bound = texture_cache.upper_bound(params.addr + params_size);

    for (auto it = texture_cache.begin(); it != texture_cache.end(); ++it) {
        CachedSurface* info = it->second.get();

        // Check if our request is contained in the surface
        if (params.addr >= info->addr && params.addr <= info->addr + info->size) {

            if (params.tiling_format != info->tiling_format ||
                params.color_format != info->color_format ||
                params.tiling_format != CachedSurface::TilingFormat::Block8x8) {

                break;
            }

            u32 bytes_per_tile = 8 * 8 * GetFormatBpp(info->color_format) / 8;
            u32 tiles_per_row = info->width / 8;

            u32 begin_tile_index = (params.addr - info->addr) / bytes_per_tile;
            int x0 = begin_tile_index % tiles_per_row * 8;
            int y0 = begin_tile_index / tiles_per_row * 8;

            state.texture_units[texture_unit].texture_2d = info->texture.handle;
            state.Apply();
            //llog LOG_WARNING(HW_GPU, "Texture reused: %x (%i,%i)-(%i,%i)", info->addr, x0, y0, x0 + params.width, y0 + params.height);
            out_rect = MathUtil::Rectangle<int>(x0, y0, x0 + params.width, y0 + params.height);
            return info;
        }
    }

    MICROPROFILE_SCOPEI("OpenGL", "Rect reuse fail", MP_RGB(128, 64, 192));
    out_rect = MathUtil::Rectangle<int>(0, 0, params.width, params.height);
    return GetSurface(state, texture_unit, params);
}

CachedSurface* SurfaceCache::LoadAndBindTexture(OpenGLState& state, unsigned texture_unit, const Pica::Regs::FullTextureConfig & config) {
    Pica::DebugUtils::TextureInfo info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config.config, config.format);

    CachedSurface params;
    params.addr = info.physical_address;
    params.width = info.width;
    params.height = info.height;
    params.tiling_format = CachedSurface::TilingFormat::Block8x8;
    params.color_format = CachedSurface::ColorFormatFromTextureFormat(info.format);
    return GetSurface(state, texture_unit, params);
}

std::tuple<CachedSurface*, CachedSurface*> SurfaceCache::LoadAndBindFramebuffer(OpenGLState& state, unsigned color_tex_unit, unsigned depth_tex_unit, const Pica::Regs::FramebufferConfig& config) {
    CachedSurface params;
    params.addr = config.GetColorBufferPhysicalAddress();
    params.width = config.GetWidth();
    params.height = config.GetHeight();
    params.tiling_format = CachedSurface::TilingFormat::Block8x8;
    params.color_format = CachedSurface::ColorFormatFromColorFormat(config.color_format);
    CachedSurface* color_surface = params.addr != 0 ? GetSurface(state, color_tex_unit, params) : nullptr;

    params.addr = config.GetDepthBufferPhysicalAddress();
    params.color_format = CachedSurface::ColorFormatFromDepthFormat(config.depth_format);
    CachedSurface* depth_surface = params.addr != 0 ? GetSurface(state, depth_tex_unit, params) : nullptr;

    return std::make_tuple(color_surface, depth_surface);
}

void SurfaceCache::InvalidateSurface(CachedSurface* surface) {
    //llog LOG_WARNING(HW_GPU, "Invalidating texture: %x", surface->addr);
    texture_cache.erase(surface->addr);
}

MICROPROFILE_DEFINE(OpenGL_FlushSurface, "OpenGL", "FlushSurface", MP_RGB(120, 120, 200));

void SurfaceCache::FlushSurface(OpenGLState& state, unsigned int texture_unit, CachedSurface* surface) {
    using ColorFormat = CachedSurface::ColorFormat;

    if (!surface->dirty) {
        return;
    }

    //llog LOG_WARNING(HW_GPU, "Flushing texture: %x", surface->addr);

    MICROPROFILE_SCOPE(OpenGL_FlushSurface);

    u8* dst_buffer = Memory::GetPhysicalPointer(surface->addr);

    state.texture_units[0].texture_2d = surface->texture.handle;
    state.Apply();
    glActiveTexture(GL_TEXTURE0 + texture_unit);

    if (surface->tiling_format == CachedSurface::TilingFormat::Linear) {
        // Only a few LCD framebuffer formats can be linear, so we'll fast-path them using OpenGL's
        // built-in texture formats.

        ASSERT((unsigned int)surface->color_format < ARRAY_SIZE(fb_format_tuples));
        const FormatTuple& tuple = fb_format_tuples[(unsigned int)surface->color_format];
        glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, dst_buffer);
    } else if (surface->tiling_format == CachedSurface::TilingFormat::Block8x8) {
        if (surface->color_format != ColorFormat::D16 &&
            surface->color_format != ColorFormat::D24 &&
            surface->color_format != ColorFormat::D24S8) {

            ASSERT((unsigned int)surface->color_format < ARRAY_SIZE(fb_format_tuples));
            const FormatTuple& tuple = fb_format_tuples[(unsigned int)surface->color_format];

            u32 bytes_per_pixel = GetFormatBpp(surface->color_format) / 8;

            std::unique_ptr<u8[]> temp_gl_buffer(new u8[surface->width * surface->height * bytes_per_pixel]);

            glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, temp_gl_buffer.get());

            // Directly copy pixels. Internal OpenGL color formats are consistent so no conversion is necessary.
            for (int y = 0; y < surface->height; ++y) {
                for (int x = 0; x < surface->width; ++x) {
                    const u32 coarse_y = y & ~7;
                    u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * surface->width * bytes_per_pixel;
                    u32 gl_pixel_index = x * bytes_per_pixel + (surface->height - 1 - y) * surface->width * bytes_per_pixel;

                    u8* pixel = dst_buffer + dst_offset;
                    memcpy(pixel, &temp_gl_buffer[gl_pixel_index], bytes_per_pixel);
                }
            }
        } else {
            // Depth/Stencil formats need special treatment since they aren't sampleable using LookupTexture and can't use RGBA format
            unsigned int tuple_idx = (unsigned int)surface->color_format - 14;
            ASSERT(tuple_idx < ARRAY_SIZE(depth_format_tuples));
            const FormatTuple& tuple = depth_format_tuples[tuple_idx];

            u32 bytes_per_pixel = GetFormatBpp(surface->color_format) / 8;

            // OpenGL needs 4 bpp alignment for D24
            u32 gl_bpp = bytes_per_pixel == 3 ? 4 : bytes_per_pixel;

            std::unique_ptr<u8[]> temp_gl_buffer(new u8[surface->width * surface->height * gl_bpp]);

            glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, temp_gl_buffer.get());

            u8* temp_gl_depth_data = bytes_per_pixel == 3 ? (temp_gl_buffer.get() + 1) : temp_gl_buffer.get();

            if (surface->color_format == ColorFormat::D24S8) {
                for (int y = 0; y < surface->height; ++y) {
                    for (int x = 0; x < surface->width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * surface->width * bytes_per_pixel;
                        u32 gl_pixel_index = (x + (surface->height - 1 - y) * surface->width);

                        u8* pixel = dst_buffer + dst_offset;
                        u32 depth_stencil = ((u32*)temp_gl_depth_data)[gl_pixel_index];
                        *(u32*)pixel = (depth_stencil >> 8) | (depth_stencil << 24);
                    }
                }
            } else {
                for (int y = 0; y < surface->height; ++y) {
                    for (int x = 0; x < surface->width; ++x) {
                        const u32 coarse_y = y & ~7;
                        u32 dst_offset = VideoCore::GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * surface->width * bytes_per_pixel;
                        u32 gl_pixel_index = (x + (surface->height - 1 - y) * surface->width) * gl_bpp;

                        u8* pixel = dst_buffer + dst_offset;
                        memcpy(pixel, &temp_gl_depth_data[gl_pixel_index], bytes_per_pixel);
                    }
                }
            }
        }
    } else if (surface->tiling_format == CachedSurface::TilingFormat::ClearPending) {
        // TODO
    }

    surface->dirty = false;
    surface->hash = Common::ComputeHash64(dst_buffer, surface->size);
}

void SurfaceCache::InvalidateInRange(PAddr addr, u32 size, bool ignore_hash) {
    // Flush any texture that falls in the flushed region
    // TODO: Optimize by also inserting upper bound (addr + size) of each texture into the same map and also narrow using lower_bound
    auto cache_upper_bound = texture_cache.upper_bound(addr + size);

    for (auto it = texture_cache.begin(); it != cache_upper_bound;) {
        const auto& info = *it->second;

        // Flush the texture only if the memory region intersects and a change is detected
        if (MathUtil::IntervalsIntersect(addr, size, info.addr, info.size) &&
            (info.dirty || ignore_hash || info.hash != Common::ComputeHash64(Memory::GetPhysicalPointer(info.addr), info.size))) {

            //llog LOG_WARNING(HW_GPU, "Invalidating texture: %x", info.addr);
            it = texture_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void SurfaceCache::FlushInRange(OpenGLState& state, unsigned texture_unit, PAddr addr, u32 size) {
    auto cache_upper_bound = texture_cache.upper_bound(addr + size);

    for (auto it = texture_cache.begin(); it != cache_upper_bound; ++it) {
        CachedSurface* info = it->second.get();

        // Flush the texture only if the memory region intersects
        if (MathUtil::IntervalsIntersect(addr, size, info->addr, info->size)) {
            FlushSurface(state, texture_unit, info);
        }
    }
}

void SurfaceCache::InvalidateAll(OpenGLState& state) {
    texture_cache.clear();
}

void SurfaceCache::FlushAll(OpenGLState& state) {
    for (auto& surface : texture_cache) {
        FlushSurface(state, 0, surface.second.get());
    }
}

}
