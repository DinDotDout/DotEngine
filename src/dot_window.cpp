#include "dot_window.hpp"

#include "SDL.h"
#include "dot_engine.hpp"
// #include <SDL_vulkan.h>

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


void DotWindow::update_extent() {
    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;
    _window_resized = true;
}

void DotWindow::handle_window_events(const SDL_Event& event) {
    switch (event.window.event) {
        case SDL_WINDOWEVENT_RESIZED:
            update_extent();
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            _minimized = true;
            break;
        case SDL_WINDOWEVENT_RESTORED:
            _minimized = false;
            break;
    }
}

DotWindow::~DotWindow() {
    SDL_DestroyWindow(_window);
}
} // namespace dt
