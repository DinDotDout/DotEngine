//> includes
#include "dot_engine.hpp"

#include <SDL.h>
#include <dot_window.hpp>

// #include <vk_initializers.h>
// #include <dot_window.hpp>
// #include <vk_types.hpp>

// #include <chrono>
#include <thread>

#include <thread>
#include <vk_types.hpp>

namespace dt {

// DotEngine* loadedEngine = nullptr;
// DotEngine& DotEngine::Get() {
//     return *loadedEngine;
// }

DotEngine::DotEngine() {
    // only one engine initialization is allowed with the application.
    // assert(loadedEngine == nullptr);
    // loadedEngine = this;

    // We initialize SDL and create a window with it.
    _dotWindow = std::make_unique<DotWindow>(initialWidth, initialHeight, "Dot Engine");
    _dotSwapchain = std::make_unique<DotSwapchain>(_dotWindow->get_window_extent(), *_dotDevice);
    _dotDevice = std::make_unique<DotDevice>(*_dotWindow);

    // everything went fine
}

void DotEngine::draw() {
    // nothing yet
}

void DotEngine::run() {
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

DotEngine::~DotEngine() {
    // cleanup
    _dotWindow.reset();
    // only one engine cleanup is allowed with the application.
    // assert(loadedEngine != nullptr);
    // loadedEngine = nullptr;
}
}
