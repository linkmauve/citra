// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>

#include <glad/glad.h>

#include "common/color.h"
#include "common/file_util.h"
#include "common/make_unique.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/profiler.h"

#include "core/memory.h"
#include "core/settings.h"
#include "core/hw/gpu.h"

#include "video_core/pica.h"
#include "video_core/utils.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/pica_to_gl.h"

static bool IsPassThroughTevStage(const Pica::Regs::TevStageConfig& stage) {
    return (stage.color_op == Pica::Regs::TevStageConfig::Operation::Replace &&
            stage.alpha_op == Pica::Regs::TevStageConfig::Operation::Replace &&
            stage.color_source1 == Pica::Regs::TevStageConfig::Source::Previous &&
            stage.alpha_source1 == Pica::Regs::TevStageConfig::Source::Previous &&
            stage.color_modifier1 == Pica::Regs::TevStageConfig::ColorModifier::SourceColor &&
            stage.alpha_modifier1 == Pica::Regs::TevStageConfig::AlphaModifier::SourceAlpha &&
            stage.GetColorMultiplier() == 1 &&
            stage.GetAlphaMultiplier() == 1);
}

RasterizerOpenGL::RasterizerOpenGL() : last_fb_color_addr(0), last_fb_depth_addr(0) { }
RasterizerOpenGL::~RasterizerOpenGL() { }

void RasterizerOpenGL::InitObjects() {
    // Create sampler objects
    for (size_t i = 0; i < texture_samplers.size(); ++i) {
        texture_samplers[i].Create();
        state.texture_units[i].sampler = texture_samplers[i].sampler.handle;
    }

    // Generate VBO, VAO and UBO
    vertex_buffer.Create();
    vertex_array.Create();
    uniform_buffer.Create();

    state.draw.vertex_array = vertex_array.handle;
    state.draw.vertex_buffer = vertex_buffer.handle;
    state.draw.uniform_buffer = uniform_buffer.handle;
    state.Apply();

    // Bind the UBO to binding point 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniform_buffer.handle);

    uniform_block_data.dirty = true;

    // Set vertex attributes
    glVertexAttribPointer(GLShader::ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, position));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_POSITION);

    glVertexAttribPointer(GLShader::ATTRIBUTE_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, color));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_COLOR);

    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord0));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord1));
    glVertexAttribPointer(GLShader::ATTRIBUTE_TEXCOORD2, 2, GL_FLOAT, GL_FALSE, sizeof(HardwareVertex), (GLvoid*)offsetof(HardwareVertex, tex_coord2));
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD0);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD1);
    glEnableVertexAttribArray(GLShader::ATTRIBUTE_TEXCOORD2);

    SetShader();

    // Configure OpenGL framebuffer
    framebuffer.Create();
    state.draw.framebuffer = framebuffer.handle;
    state.Apply();

    //ASSERT_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
    //           "OpenGL rasterizer framebuffer setup failed, status %X", glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

void RasterizerOpenGL::Reset() {
    SyncCullMode();
    SyncBlendEnabled();
    SyncBlendFuncs();
    SyncBlendColor();
    SyncLogicOp();
    SyncStencilTest();
    SyncDepthTest();

    SetShader();

    res_cache.InvalidateAll(state);
}

void RasterizerOpenGL::AddTriangle(const Pica::Shader::OutputVertex& v0,
                                   const Pica::Shader::OutputVertex& v1,
                                   const Pica::Shader::OutputVertex& v2) {
    vertex_batch.emplace_back(v0);
    vertex_batch.emplace_back(v1);
    vertex_batch.emplace_back(v2);
}

void RasterizerOpenGL::DrawTriangles() {
    SyncFramebuffer();
    SyncDrawState();

    if (state.draw.shader_dirty) {
        SetShader();
        state.draw.shader_dirty = false;
    }

    if (uniform_block_data.dirty) {
        glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformData), &uniform_block_data.data, GL_STATIC_DRAW);
        uniform_block_data.dirty = false;
    }

    glBufferData(GL_ARRAY_BUFFER, vertex_batch.size() * sizeof(HardwareVertex), vertex_batch.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_batch.size());
    glFlush();

    vertex_batch.clear();

    color_surface->dirty = true;
    depth_surface->dirty = true;
}

void RasterizerOpenGL::FlushAllSurfaces() {
    res_cache.FlushAll(state);
}

void RasterizerOpenGL::NotifyPicaRegisterChanged(u32 id) {
    const auto& regs = Pica::g_state.regs;

    if (!Settings::values.use_hw_renderer)
        return;

    switch(id) {
    // Culling
    case PICA_REG_INDEX(cull_mode):
        SyncCullMode();
        break;

    // Blending
    case PICA_REG_INDEX(output_merger.alphablend_enable):
        SyncBlendEnabled();
        break;
    case PICA_REG_INDEX(output_merger.alpha_blending):
        SyncBlendFuncs();
        break;
    case PICA_REG_INDEX(output_merger.blend_const):
        SyncBlendColor();
        break;

    // Alpha test
    case PICA_REG_INDEX(output_merger.alpha_test):
        SyncAlphaTest();
        state.draw.shader_dirty = true;
        break;

    // Stencil test
    case PICA_REG_INDEX(output_merger.stencil_test.raw_func):
    case PICA_REG_INDEX(output_merger.stencil_test.raw_op):
        SyncStencilTest();
        break;

    // Depth test
    case PICA_REG_INDEX(output_merger.depth_test_enable):
        SyncDepthTest();
        break;

    // Logic op
    case PICA_REG_INDEX(output_merger.logic_op):
        SyncLogicOp();
        break;

    // TEV stages
    case PICA_REG_INDEX(tev_stage0.color_source1):
    case PICA_REG_INDEX(tev_stage0.color_modifier1):
    case PICA_REG_INDEX(tev_stage0.color_op):
    case PICA_REG_INDEX(tev_stage0.color_scale):
    case PICA_REG_INDEX(tev_stage1.color_source1):
    case PICA_REG_INDEX(tev_stage1.color_modifier1):
    case PICA_REG_INDEX(tev_stage1.color_op):
    case PICA_REG_INDEX(tev_stage1.color_scale):
    case PICA_REG_INDEX(tev_stage2.color_source1):
    case PICA_REG_INDEX(tev_stage2.color_modifier1):
    case PICA_REG_INDEX(tev_stage2.color_op):
    case PICA_REG_INDEX(tev_stage2.color_scale):
    case PICA_REG_INDEX(tev_stage3.color_source1):
    case PICA_REG_INDEX(tev_stage3.color_modifier1):
    case PICA_REG_INDEX(tev_stage3.color_op):
    case PICA_REG_INDEX(tev_stage3.color_scale):
    case PICA_REG_INDEX(tev_stage4.color_source1):
    case PICA_REG_INDEX(tev_stage4.color_modifier1):
    case PICA_REG_INDEX(tev_stage4.color_op):
    case PICA_REG_INDEX(tev_stage4.color_scale):
    case PICA_REG_INDEX(tev_stage5.color_source1):
    case PICA_REG_INDEX(tev_stage5.color_modifier1):
    case PICA_REG_INDEX(tev_stage5.color_op):
    case PICA_REG_INDEX(tev_stage5.color_scale):
    case PICA_REG_INDEX(tev_combiner_buffer_input):
        state.draw.shader_dirty = true;
        break;
    case PICA_REG_INDEX(tev_stage0.const_r):
        SyncTevConstColor(0, regs.tev_stage0);
        break;
    case PICA_REG_INDEX(tev_stage1.const_r):
        SyncTevConstColor(1, regs.tev_stage1);
        break;
    case PICA_REG_INDEX(tev_stage2.const_r):
        SyncTevConstColor(2, regs.tev_stage2);
        break;
    case PICA_REG_INDEX(tev_stage3.const_r):
        SyncTevConstColor(3, regs.tev_stage3);
        break;
    case PICA_REG_INDEX(tev_stage4.const_r):
        SyncTevConstColor(4, regs.tev_stage4);
        break;
    case PICA_REG_INDEX(tev_stage5.const_r):
        SyncTevConstColor(5, regs.tev_stage5);
        break;

    // TEV combiner buffer color
    case PICA_REG_INDEX(tev_combiner_buffer_color):
        SyncCombinerColor();
        break;
    }
}

void RasterizerOpenGL::NotifyPreRead(PAddr addr, u32 size) {
    const auto& regs = Pica::g_state.regs;

    if (!Settings::values.use_hw_renderer)
        return;

    // Notify cache of flush in case the region touches a cached resource
    res_cache.FlushInRange(state, addr, size);
}

void RasterizerOpenGL::NotifyFlush(PAddr addr, u32 size) {
    const auto& regs = Pica::g_state.regs;

    if (!Settings::values.use_hw_renderer)
        return;

    // Notify cache of flush in case the region touches a cached resource
    res_cache.InvalidateInRange(addr, size);
}

void RasterizerOpenGL::SamplerInfo::Create() {
    sampler.Create();
    mag_filter = min_filter = TextureConfig::Linear;
    wrap_s = wrap_t = TextureConfig::Repeat;
    border_color = 0;

    glSamplerParameteri(sampler.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // default is GL_LINEAR_MIPMAP_LINEAR
    // Other attributes have correct defaults
}

void RasterizerOpenGL::SamplerInfo::SyncWithConfig(const Pica::Regs::TextureConfig& config) {
    GLuint s = sampler.handle;

    if (mag_filter != config.mag_filter) {
        mag_filter = config.mag_filter;
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, PicaToGL::TextureFilterMode(mag_filter));
    }
    if (min_filter != config.min_filter) {
        min_filter = config.min_filter;
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, PicaToGL::TextureFilterMode(min_filter));
    }

    if (wrap_s != config.wrap_s) {
        wrap_s = config.wrap_s;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, PicaToGL::WrapMode(wrap_s));
    }
    if (wrap_t != config.wrap_t) {
        wrap_t = config.wrap_t;
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, PicaToGL::WrapMode(wrap_t));
    }

    if (wrap_s == TextureConfig::ClampToBorder || wrap_t == TextureConfig::ClampToBorder) {
        if (border_color != config.border_color.raw) {
            auto gl_color = PicaToGL::ColorRGBA8(border_color);
            glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, gl_color.data());
        }
    }
}

void RasterizerOpenGL::SetShader() {
    PicaShaderConfig config = PicaShaderConfig::CurrentConfig();
    std::unique_ptr<PicaShader> shader = Common::make_unique<PicaShader>();

    // Find (or generate) the GLSL shader for the current TEV state
    auto cached_shader = shader_cache.find(config);
    if (cached_shader != shader_cache.end()) {
        current_shader = cached_shader->second.get();

        state.draw.shader_program = current_shader->shader.handle;
        state.Apply();
    } else {
        LOG_DEBUG(Render_OpenGL, "Creating new shader");

        shader->shader.Create(GLShader::GenerateVertexShader().c_str(), GLShader::GenerateFragmentShader(config).c_str());

        state.draw.shader_program = shader->shader.handle;
        state.Apply();

        // Set the texture samplers to correspond to different texture units
        GLuint uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[0]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 0); }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[1]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 1); }
        uniform_tex = glGetUniformLocation(shader->shader.handle, "tex[2]");
        if (uniform_tex != -1) { glUniform1i(uniform_tex, 2); }

        current_shader = shader_cache.emplace(config, std::move(shader)).first->second.get();

        unsigned int block_index = glGetUniformBlockIndex(current_shader->shader.handle, "shader_data");
        glUniformBlockBinding(current_shader->shader.handle, block_index, 0);
    }

    // Update uniforms
    SyncAlphaTest();
    SyncCombinerColor();
    auto& tev_stages = Pica::g_state.regs.GetTevStages();
    for (int index = 0; index < tev_stages.size(); ++index)
        SyncTevConstColor(index, tev_stages[index]);
}

void RasterizerOpenGL::SyncFramebuffer() {
    using RendererGL::CachedSurface;

    const auto& regs = Pica::g_state.regs;

    std::tie(color_surface, depth_surface) = res_cache.LoadAndBindFramebuffer(state, 0, 0, regs.framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_surface->texture.handle, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_surface->texture.handle, 0);
    bool has_stencil = regs.framebuffer.depth_format == Pica::Regs::DepthFormat::D24S8;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, has_stencil ? depth_surface->texture.handle : 0, 0);
}

void RasterizerOpenGL::SyncCullMode() {
    const auto& regs = Pica::g_state.regs;

    switch (regs.cull_mode) {
    case Pica::Regs::CullMode::KeepAll:
        state.cull.enabled = false;
        break;

    case Pica::Regs::CullMode::KeepClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CW;
        break;

    case Pica::Regs::CullMode::KeepCounterClockWise:
        state.cull.enabled = true;
        state.cull.front_face = GL_CCW;
        break;

    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown cull mode %d", regs.cull_mode.Value());
        UNIMPLEMENTED();
        break;
    }
}

void RasterizerOpenGL::SyncBlendEnabled() {
    state.blend.enabled = (Pica::g_state.regs.output_merger.alphablend_enable == 1);
}

void RasterizerOpenGL::SyncBlendFuncs() {
    const auto& regs = Pica::g_state.regs;
    state.blend.src_rgb_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_source_rgb);
    state.blend.dst_rgb_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_dest_rgb);
    state.blend.src_a_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_source_a);
    state.blend.dst_a_func = PicaToGL::BlendFunc(regs.output_merger.alpha_blending.factor_dest_a);
}

void RasterizerOpenGL::SyncBlendColor() {
    auto blend_color = PicaToGL::ColorRGBA8(Pica::g_state.regs.output_merger.blend_const.raw);
    state.blend.color.red = blend_color[0];
    state.blend.color.green = blend_color[1];
    state.blend.color.blue = blend_color[2];
    state.blend.color.alpha = blend_color[3];
}

void RasterizerOpenGL::SyncAlphaTest() {
    const auto& regs = Pica::g_state.regs;
    if (regs.output_merger.alpha_test.ref != uniform_block_data.data.alphatest_ref) {
        uniform_block_data.data.alphatest_ref = regs.output_merger.alpha_test.ref;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncLogicOp() {
    state.logic_op = PicaToGL::LogicOp(Pica::g_state.regs.output_merger.logic_op);
}

void RasterizerOpenGL::SyncStencilTest() {
    const auto& regs = Pica::g_state.regs;
    state.stencil.test_enabled = regs.output_merger.stencil_test.enable && regs.framebuffer.depth_format == Pica::Regs::DepthFormat::D24S8;
    state.stencil.test_func = PicaToGL::CompareFunc(regs.output_merger.stencil_test.func);
    state.stencil.test_ref = regs.output_merger.stencil_test.reference_value;
    state.stencil.test_mask = regs.output_merger.stencil_test.input_mask;
    state.stencil.write_mask = regs.output_merger.stencil_test.write_mask;
    state.stencil.action_stencil_fail = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_stencil_fail);
    state.stencil.action_depth_fail = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_depth_fail);
    state.stencil.action_depth_pass = PicaToGL::StencilOp(regs.output_merger.stencil_test.action_depth_pass);
}

void RasterizerOpenGL::SyncDepthTest() {
    const auto& regs = Pica::g_state.regs;
    state.depth.test_enabled = (regs.output_merger.depth_test_enable == 1);
    state.depth.test_func = PicaToGL::CompareFunc(regs.output_merger.depth_test_func);
    state.color_mask.red_enabled = regs.output_merger.red_enable;
    state.color_mask.green_enabled = regs.output_merger.green_enable;
    state.color_mask.blue_enabled = regs.output_merger.blue_enable;
    state.color_mask.alpha_enabled = regs.output_merger.alpha_enable;
    state.depth.write_mask = regs.output_merger.depth_write_enable ? GL_TRUE : GL_FALSE;
}

void RasterizerOpenGL::SyncCombinerColor() {
    auto combiner_color = PicaToGL::ColorRGBA8(Pica::g_state.regs.tev_combiner_buffer_color.raw);
    if (combiner_color != uniform_block_data.data.tev_combiner_buffer_color) {
        uniform_block_data.data.tev_combiner_buffer_color = combiner_color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncTevConstColor(int stage_index, const Pica::Regs::TevStageConfig& tev_stage) {
    auto const_color = PicaToGL::ColorRGBA8(tev_stage.const_color);
    if (const_color != uniform_block_data.data.const_color[stage_index]) {
        uniform_block_data.data.const_color[stage_index] = const_color;
        uniform_block_data.dirty = true;
    }
}

void RasterizerOpenGL::SyncDrawState() {
    const auto& regs = Pica::g_state.regs;

    // Sync the viewport
    GLsizei viewport_width = (GLsizei)Pica::float24::FromRawFloat24(regs.viewport_size_x).ToFloat32() * 2;
    GLsizei viewport_height = (GLsizei)Pica::float24::FromRawFloat24(regs.viewport_size_y).ToFloat32() * 2;

    // TODO: Use floating-point viewports for accuracy if supported
    glViewport((GLsizei)regs.viewport_corner.x,
               (GLsizei)regs.viewport_corner.y,
                viewport_width, viewport_height);

    // Sync bound texture(s), upload if not cached
    const auto pica_textures = regs.GetTextures();
    for (unsigned texture_index = 0; texture_index < pica_textures.size(); ++texture_index) {
        const auto& texture = pica_textures[texture_index];

        if (texture.enabled) {
            texture_samplers[texture_index].SyncWithConfig(texture.config);
            res_cache.LoadAndBindTexture(state, texture_index, texture);
        } else {
            state.texture_units[texture_index].texture_2d = 0;
        }
    }

    state.draw.uniform_buffer = uniform_buffer.handle;
    state.Apply();
}
