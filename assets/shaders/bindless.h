#ifndef BINDLESS_H
#define BINDLESS_H

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

#define GLOBAL_BINDLESS_SET 0
#define GLOBAL_SAMPLER_COUNT 8 
#define GLOBAL_SAMPLER_BINDING 0
#define GLOBAL_IMAGE_COUNT 8
#define GLOBAL_IMAGE_BINDING 1

layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform sampler2D global_textures[GLOBAL_SAMPLER_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform usampler2D global_textures_uint[GLOBAL_SAMPLER_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform sampler3D global_textures_3d[GLOBAL_SAMPLER_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform usampler3D global_textures_3d_uint[GLOBAL_SAMPLER_COUNT];

layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba8) uniform image2D global_images_2d_rgba8[GLOBAL_IMAGE_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba16f) uniform image2D global_images_2d_rgba16f[GLOBAL_IMAGE_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba32f) uniform image2D global_images_2d_rgba32f[GLOBAL_IMAGE_COUNT];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, r32f) uniform image2D global_images_2d_r32f[GLOBAL_IMAGE_COUNT];

#endif
