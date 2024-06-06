#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>

#include "dot_window.hpp"
#include "vk_renderer/utils/deletion_queue.hpp"
#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/vk_types.hpp" // Vma allocator needed

namespace dt {

class DotSwapchain {
    struct FrameData {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;
        DeletionQueue deletionQueue{};
    };

  public:
    DotSwapchain(const DotWindow& dotWindow,
                 const DotDevice& dotDevice,
                 const VkSurfaceKHR& surface);
    ~DotSwapchain();

    DotSwapchain(const DotSwapchain&) = delete;
    DotSwapchain& operator=(const DotSwapchain&) = delete;

    VkSwapchainKHR get_swapchain() const { return _swapchain; }
    VkImage* get_image() { return &_swapchainImages[_swapchainImageIndex]; }
    const VkExtent2D& get_swapchain_extent() { return _swapchainExtent; }

    uint32_t get_current_frame() const { return _frameNumber; }
    FrameData& get_current_frame_data() { return _frames[_frameNumber % FRAMES_IN_FLIGHT]; }
    void next_frame() { _frameNumber++; }

    void resize_swapchain(VkExtent2D newExtent);
    bool acquire_next_image(VkImage** imageIndex); // NOTE: May need to revisit this.
    bool swapchain_present_image(VkQueue queue);

    void submit_command_buffer(VkCommandBuffer* cmd);

  private:
    static constexpr unsigned int FRAMES_IN_FLIGHT = 2;
    FrameData _frames[FRAMES_IN_FLIGHT];
    uint32_t _frameNumber = 0;
    uint32_t _swapchainImageIndex;

    VkExtent2D _swapchainExtent;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    const VkSurfaceKHR& _surface;
    const DotDevice& _dotDevice;
    const DotWindow& _dotWindow;

    void create_swapchain(VkExtent2D newExtent);
    void destroy_swapchain();

    // NOTE: Keep separate from command buffers in case of future changes
    void create_command_pools();
    void allocate_command_buffers();

    void create_sync_structures();
};
} // namespace dt
