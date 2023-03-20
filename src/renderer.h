#ifndef CGFS_RENDERER_H
#define CGFS_RENDERER_H

#include "renderer_vulkan.h"
#include "window.h"

Renderer renderer_create(
        Window window,
        usize vertex_shader_length,
        const u32 *vertex_shader_spv,
        usize fragment_shader_length,
        const u32 *fragment_shader_spv
);

void renderer_reload(Renderer renderer);

void renderer_destroy(Renderer renderer);

void renderer_draw_frame(Renderer renderer);

#endif //CGFS_RENDERER_H
