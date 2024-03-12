//> includes
#include "dot_engine.hpp"

#include <SDL.h>
#include <thread>

namespace dt {

void DotEngine::run() {
    SDL_Event e;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                vkDeviceWaitIdle(_dotDevice.get_device());
                return;
            }
            if (e.type == SDL_WINDOWEVENT) {
                _dotWindow.handle_window_events(e);
            }
        }

        if (_dotWindow.should_stop_rendering()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (_dotWindow.is_resize_requested()) {
            _dotRenderer.resize_swapchain();
            _dotWindow.reset_resize_request();
        }
        _dotRenderer.render_pass();
    }
}

}
