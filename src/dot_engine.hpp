// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "dot_swapchain.hpp"
#include <cstdint>
#include <memory>

#include <dot_device.hpp>
#include <dot_window.hpp>

namespace dt {
class DotEngine {

  public:
    DotEngine();
    ~DotEngine();
    static constexpr uint32_t initialHeight = 900;
    static constexpr uint32_t initialWidth = 1700;

    int _frameNumber{ 0 };
    bool stop_rendering{ false };

    static DotEngine& Get();

    // run main loop
    void run();

  private:
    std::unique_ptr<DotWindow> _dotWindow;
    std::unique_ptr<DotDevice> _dotDevice;
    std::unique_ptr<DotSwapchain> _dotSwapchain;
    // draw loop
    void draw();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
};
}
