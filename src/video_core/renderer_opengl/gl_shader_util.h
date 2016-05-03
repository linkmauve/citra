// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>

namespace GLShader {

enum Attributes {
    ATTRIBUTE_POSITION,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_TEXCOORD0,
    ATTRIBUTE_TEXCOORD1,
    ATTRIBUTE_TEXCOORD2,
};

/**
 * Utility function to create and compile an OpenGL GLSL shader program (vertex + fragment shader)
 * @param vertex_shader String of the GLSL vertex shader program
 * @param fragment_shader String of the GLSL fragment shader program
 * @returns Handle of the newly created OpenGL program object
 */
GLuint LoadProgram(const char* vertex_shader, const char* fragment_shader);

/**
 * Utility function to create and compile an OpenGL GLSL shader program (compute shader)
 * @param compute_shader String of the GLSL compute shader program
 * @returns Handle of the newly created OpenGL program object
 */
GLuint LoadComputeProgram(const char* compute_shader);

} // namespace
