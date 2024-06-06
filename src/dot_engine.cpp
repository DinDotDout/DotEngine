#include "dot_engine.hpp"
#include <SDL.h>
#include <thread>

namespace dt {
void DotEngine::run() {
    SDL_Event e;
    bool bQuit = false;
    bool resize_= false;
    while (!bQuit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                _dotRenderer.wait_device_idle();
                return;
            }
            if (e.type == SDL_WINDOWEVENT) {
                _dotWindow.handle_window_events(e);
            }
        }

        if (_dotWindow.is_minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_dotWindow.is_resize_requested()) {
            _dotRenderer.resize_swapchain();
            _dotWindow.reset_resized();
        }
        _dotRenderer.render_pass();
    }
}
}  // namespace dt
