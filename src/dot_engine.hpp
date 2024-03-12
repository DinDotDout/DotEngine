// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "dot_device.hpp"
#include "dot_renderer.hpp"
// #include "dot_window.hpp"

class DotDevice;
class DotWindow;
class DotRenderer;

namespace dt {
class DotEngine {

  public:
    DotEngine() = default;
    ~DotEngine() = default;
    static constexpr uint32_t INITIAL_HEIGHT = 900;
    static constexpr uint32_t INITIAL_WIDTH = 1700;

    static DotEngine& Get();
    void run();

  private:
    DotWindow _dotWindow{ INITIAL_WIDTH, INITIAL_HEIGHT, "Dot Engine" };
    DotDevice _dotDevice{ _dotWindow };
    DotRenderer _dotRenderer{ _dotWindow, _dotDevice };
};
}
