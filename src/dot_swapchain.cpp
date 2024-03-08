#include "dot_swapchain.hpp"
#include "VkBootstrap.h"

namespace dt {

DotSwapchain::DotSwapchain(VkExtent2D windowExtent, DotDevice& dotDeviceRef)
  : _swapchainExtent{ windowExtent }
  , _dotDevice{ dotDeviceRef } {
    create_swapchain(_swapchainExtent);
}

void DotSwapchain::create_swapchain(VkExtent2D swapchainExtent) {
    vkb::SwapchainBuilder swapchainBuilder{ _dotDevice.get_gpu(),
                                            _dotDevice.get_device(),
                                            _dotDevice.get_surface() };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
      swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat,
                                                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        // use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(swapchainExtent.width, swapchainExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    // store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

DotSwapchain::~DotSwapchain() {
    vkDestroySwapchainKHR(_dotDevice.get_device(), _swapchain, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_dotDevice.get_device(), _swapchainImageViews[i], nullptr);
    }
}
}
