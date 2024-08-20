#pragma once

#include "camera.hpp"
#include "dot_window.hpp"
#include "vk_renderer/core/vk_backend.hpp"
#include <vulkan/vulkan_core.h>

namespace dt {

class DotImGui;

struct EngineStats {
    float frameTime;
};

class DotEngine {
  public:
    DotEngine();
    ~DotEngine() = default;

    static constexpr uint32_t INITIAL_HEIGHT = 900;
    static constexpr uint32_t INITIAL_WIDTH = 1700;
    EngineStats stats;

    static DotEngine& get();
    void run();

  private:
    Camera mainCamera;
    DotWindow _dotWindow{ INITIAL_WIDTH, INITIAL_HEIGHT, "Dot Engine" };
    VkBackend _vkBackend{ _dotWindow };
    // std::unique_ptr<DotImGui> _dotImGui;

    void _draw_imgui();
    void _update_scene();
};
} // namespace dt
