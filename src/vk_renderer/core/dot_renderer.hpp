#pragma once

#include "./dot_window.hpp"
#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/core/dot_pipeline.hpp"
#include "vk_renderer/core/dot_swapchain.hpp"
#include "memory"

namespace dt {
class DotRenderer {
    struct AllocatedImage {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkExtent3D imageExtent;
        VkFormat imageFormat;
    };

  public:
    explicit DotRenderer(DotWindow& dotWindow);
    ~DotRenderer();

    DotRenderer(const DotRenderer&) = delete;
    DotRenderer& operator=(const DotRenderer&) = delete;

    void render_pass();
    void resize_swapchain();
    void wait_device_idle();

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
    DeletionQueue _deletionQueue{};

    VmaAllocator _allocator;
    AllocatedImage _drawImage;
    VkExtent2D _drawExtent;

    AllocatedImage _depthImage;

    void _init_vulkan();
    vkb::Instance _create_instance();
    bool _create_surface();
    void _init_device(vkb::Instance vkb_inst);
    void _init_swapchain();
    void _init_allocator();
    void _init_pipeline();
    void _create_draw_image_and_view();

    // void draw(VkCommandBuffer cmd, VkImage* im);
    void draw(VkCommandBuffer cmd);
    void _end_command_buffer(VkCommandBuffer* cmd);
    void _begin_command_buffer(VkCommandBuffer* cmd);
};
} // namespace dt
