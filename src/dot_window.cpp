#include "dot_window.hpp"

#include "SDL.h"
#include <SDL_vulkan.h>

namespace dt {

DotWindow::DotWindow(uint32_t w, uint32_t h, std::string name)
  : _windowExtent{ w, h }
  , _name{ name } {
    SDL_Init(SDL_INIT_VIDEO);

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

void DotWindow::handle_window_events(const SDL_Event& event) {
    switch (event.window.event) {
        case SDL_WINDOWEVENT_RESIZED:
            _just_resized = true;
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            _stop_rendering = true;
            break;
        case SDL_WINDOWEVENT_RESTORED:
            _stop_rendering = false;
            break;
    }
}

DotWindow::~DotWindow() {
    SDL_DestroyWindow(_window);
}
}
