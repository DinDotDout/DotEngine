#pragma once

#include <dot_device.hpp>
#include <vk_types.hpp>

namespace dt {

class DotSwapchain {

  public:
    DotSwapchain(VkExtent2D windowExtent, DotDevice& dotDeviceRef);

    DotSwapchain(DotSwapchain&&) = default;
    DotSwapchain(const DotSwapchain&) = default;

    DotSwapchain& operator=(DotSwapchain&&) = delete;
    DotSwapchain& operator=(const DotSwapchain&) = delete;

    unsigned int get_frames_in_flight() const { return FRAMES_IN_FLIGHT; }
    ~DotSwapchain();

  private:
    struct FrameData {
        VkCommandPool _commandPool;
        VkCommandBuffer _mainCommandBuffer;
    };

    const unsigned int FRAMES_IN_FLIGHT = 2;

    VkExtent2D _swapchainExtent;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    DotDevice& _dotDevice;

    void create_swapchain(VkExtent2D);
    void destroy_swapchain();
};
}
