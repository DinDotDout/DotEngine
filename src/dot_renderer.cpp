#include "dot_renderer.hpp"

namespace dt {
DotRenderer::DotRenderer(DotWindow& dotWindow, DotDevice& dotDevice)
  : _dotWindow(dotWindow)
  , _dotDevice(dotDevice)
  , _dotSwapchain(_dotWindow.get_window_extent(), _dotDevice) {}

void DotRenderer::render_pass() {
    auto device = _dotDevice.get_device();
    auto frameData = _dotSwapchain.get_current_frame_data();

    uint32_t swapchainImageIndex;
    VkResult e = _dotSwapchain.acquire_next_image(&swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        _dotWindow.request_resize();
        return;
    }

    VkImage currentImage = _dotSwapchain.get_image(swapchainImageIndex);
    VkCommandBuffer cmd = frameData.mainCommandBuffer;

    record_command_buffer(&cmd, &currentImage);
    draw(cmd, currentImage);
    _dotSwapchain.submit_command_buffer(&cmd, swapchainImageIndex);

    VkResult presentResult =
      _dotSwapchain.present(_dotDevice.get_graphics_queue(), swapchainImageIndex);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        _dotWindow.request_resize();
    }

    _dotSwapchain.next_frame();
}

void DotRenderer::record_command_buffer(VkCommandBuffer* cmd, VkImage* swapchainImage) {
    VK_CHECK(vkResetCommandBuffer(*cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo =
      _dotDevice.command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(*cmd, &cmdBeginInfo));
    // VkImage currentImage = _dotSwapchain.get_image(swapchainImageIndex);
    _dotDevice.transition_image(
      *cmd, *swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

// Move to swapchain and only pass idx (it anything at all)

void DotRenderer::draw(VkCommandBuffer cmd, VkImage currentImage) {
    uint32_t frameNumber = _dotSwapchain.get_current_frame();
    VkClearColorValue clearValue;
    float flash = abs(sin(frameNumber / 120.f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };
    VkImageSubresourceRange clearRange =
      _dotDevice.image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, currentImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void DotRenderer::resize_swapchain() {
    _dotSwapchain.resize_swapchain(_dotWindow.get_window_extent());
}
}
