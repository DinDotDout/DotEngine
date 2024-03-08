#include "dot_window.hpp"

#include "SDL.h"
#include <SDL_vulkan.h>

namespace dt {

DotWindow::DotWindow(uint32_t w, uint32_t h, std::string name)
  : _windowExtent{ w, h }
  , _name{ name } {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(_name.c_str(),
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               _windowExtent.width,
                               _windowExtent.height,
                               window_flags);
}

void DotWindow::create_surface(VkInstance instance, VkSurfaceKHR* surface) {
    SDL_Vulkan_CreateSurface(_window, instance, surface);
}

DotWindow::~DotWindow() {
    SDL_DestroyWindow(_window);
}
}
