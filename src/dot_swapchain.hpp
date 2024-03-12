#pragma once

#include <cstdint>
#include <dot_device.hpp>

#include <vk_types.hpp>
#include <vulkan/vulkan_core.h>

namespace dt {

class DotSwapchain {

    struct AllocatedImage {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkExtent3D imageExtent;
        VkFormat imageFormat;
    };

    struct FrameData {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;
        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;
    };

  public:
    DotSwapchain(VkExtent2D windowExtent, DotDevice& dotDeviceRef);
    ~DotSwapchain();

    DotSwapchain(const DotSwapchain&) = delete;
    DotSwapchain& operator=(const DotSwapchain&) = delete;

    VkSwapchainKHR get_swapchain() const { return _swapchain; }
    VkImage get_image(uint32_t i) const { return _swapchainImages[i]; }
    void resize_swapchain(VkExtent2D newExtent);

    VkResult acquire_next_image(uint32_t* imageIndex);

    VkResult present(VkQueue queue, uint32_t imageIndex);

    uint32_t get_current_frame() const { return _frameNumber; }
    FrameData& get_current_frame_data() { return _frames[_frameNumber % FRAMES_IN_FLIGHT]; };
    void next_frame() { _frameNumber++; };

    void submit_command_buffer(VkCommandBuffer* cmd, uint32_t imageIndex);

  private:
    static constexpr unsigned int FRAMES_IN_FLIGHT = 2;
    FrameData _frames[FRAMES_IN_FLIGHT];
    uint32_t _frameNumber = 0;

    VkExtent2D _swapchainExtent;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

    DotDevice& _dotDevice;
    VmaAllocator _allocator; // vma lib allocator

    void create_swapchain(VkExtent2D newExtent);
    void init_allocator();
    void create_image_views();
    void destroy_swapchain();
    void init_commands();
    void init_sync_structures();
};
}
