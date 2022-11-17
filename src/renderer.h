#ifndef CGFS_RENDERER_H
#define CGFS_RENDERER_H

#include "renderer_vulkan.h"
#include "window.h"

Renderer renderer_create(Window window);

void renderer_destroy(Renderer renderer);

#endif //CGFS_RENDERER_H
