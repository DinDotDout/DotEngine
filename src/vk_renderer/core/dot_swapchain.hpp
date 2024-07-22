#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>
// #include "vk_renderer/core/dot_device.hpp"
// #include "dot_window.hpp"
#include "vk_renderer/core/dot_descriptors.hpp"
#include "vk_renderer/utils/deletion_queue.hpp"

namespace dt {
class DotDevice;
class DotWindow;

class DotSwapchain {
    struct FrameData {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;
        DeletionQueue deletionQueue{};
        DescriptorAllocatorGrowable frameDescriptors{};
    };

  public:
    DotSwapchain(const DotWindow& dotWindow,
                 const DotDevice& dotDevice,
                 const VkSurfaceKHR& surface);
    ~DotSwapchain();

    DotSwapchain(const DotSwapchain&) = delete;
    DotSwapchain& operator=(const DotSwapchain&) = delete;

    VkSwapchainKHR get_swapchain() const { return _swapchain; }
    VkExtent2D get_swapchain_extent() { return _swapchainExtent; }
    VkFormat get_swapchain_image_format() { return _swapchainImageFormat; }

    uint32_t get_current_frame() const { return _frame % FRAMES_IN_FLIGHT; }
    // uint32_t get_current_frame() const { return _frame; }
    FrameData& get_current_frame_data() { return _frames[_frame % FRAMES_IN_FLIGHT]; }
    // void next_frame() { _frame++; }

    void resize_swapchain(VkExtent2D newExtent);
    bool acquire_next_image(VkImage* image,
                            VkImageView* imageView); // NOTE: May need to revisit this.
    bool swapchain_present_image(VkQueue queue);
    void submit_command_buffer(VkCommandBuffer cmd);
    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    void clear_frames();

  private:
    static constexpr unsigned int FRAMES_IN_FLIGHT = 2;
    FrameData _frames[FRAMES_IN_FLIGHT];
    uint32_t _frame = 0;
    uint32_t _swapchainImageIndex;

    VkExtent2D _swapchainExtent;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    VkFence _immediateFence;
    VkCommandPool _immediateCommandPool;
    VkCommandBuffer _immediateCommandBuffer;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    const VkSurfaceKHR& _surface;
    const DotDevice& _dotDevice;
    const DotWindow& _dotWindow;

    DeletionQueue _deletionQueue{};
    void create_swapchain(VkExtent2D newExtent);
    void destroy_swapchain();

    // NOTE: Keep separate from command buffers in case of future changes
    void create_frame_command_pools();
    void allocate_frame_command_buffers();
    void init_descriptor_pools();
    void create_frame_sync_structures();
    void create_immediate_submit_structures();
};
} // namespace dt
