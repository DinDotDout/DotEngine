#pragma once

#include "dot_window.hpp"
#include "vk_renderer/core/vk_backend.hpp"
#include "camera.hpp"
#include <vulkan/vulkan_core.h>

namespace dt {

class DotEngine {
  public:
    DotEngine();
    ~DotEngine() = default;

    static constexpr uint32_t INITIAL_HEIGHT = 900;
    static constexpr uint32_t INITIAL_WIDTH = 1700;

    static DotEngine& Get();
    void run();

  private:
    Camera mainCamera;
    DotWindow _dotWindow{ INITIAL_WIDTH, INITIAL_HEIGHT, "Dot Engine" };
    VkBackend _vkBackend{ _dotWindow };
};
} // namespace dt
