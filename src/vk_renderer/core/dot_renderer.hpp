#pragma once

#include "./dot_window.hpp"
#include "memory"
#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/core/dot_imgui.hpp"
#include "vk_renderer/core/dot_pipeline.hpp"
#include "vk_renderer/core/dot_swapchain.hpp"
#include "vk_renderer/vk_types.hpp" // Vma allocator needed
#include "vk_renderer/utils/dot_images.hpp"
#include <iostream>

namespace dt {
class DotRenderer {
    // NOTE: Should layout be included?

  public:
    explicit DotRenderer(DotWindow& dotWindow);
    ~DotRenderer();

    DotRenderer(const DotRenderer&) = delete;
    DotRenderer& operator=(const DotRenderer&) = delete;

    void render_pass();
    void resize_swapchain();
    void wait_device_idle();
    // DotPipeline::ComputeEffect get_effects();
    const std::vector<DotPipeline::ComputeEffect>& get_background_effects();
    int get_current_background_effect();
    void set_current_background_effect(int backgroundEffect) {
        _dotPipeline->set_current_background_effect(backgroundEffect);
    }

  private:
    DotWindow& _dotWindow;
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool _useValidationLayers = true;
#endif
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkSurfaceKHR _surface{};
    VkInstance _instance{};

    std::unique_ptr<DotDevice> _dotDevice;
    std::unique_ptr<DotSwapchain> _dotSwapchain;
    std::unique_ptr<DotPipeline> _dotPipeline;
    std::unique_ptr<DotImGui> _dotImGui;

    DeletionQueue _deletionQueue{};

    VmaAllocator _allocator;
    vkutil::AllocatedImage _drawImage;
    VkExtent2D _drawExtent;

    vkutil::AllocatedImage _depthImage;

    void _init_vulkan();
    vkb::Instance _create_instance();
    bool _create_surface();
    void _init_device(vkb::Instance vkb_inst);
    void _init_swapchain();
    void _init_allocator();
    void _init_pipeline();
    void _init_imgui();
    void _create_draw_image_and_view();

    // void draw(VkCommandBuffer cmd, VkImage* im);
    void draw_background(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void draw_geometry(VkCommandBuffer cmd);
    void _end_command_buffer(VkCommandBuffer* cmd);
    void _begin_command_buffer(VkCommandBuffer* cmd);
};
} // namespace dt
