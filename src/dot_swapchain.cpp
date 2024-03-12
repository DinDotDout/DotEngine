#include "dot_swapchain.hpp"
#include "VkBootstrap.h"
#include "dot_initializers.hpp"
#include <cstdint>

namespace dt {

DotSwapchain::DotSwapchain(VkExtent2D windowExtent, DotDevice& dotDeviceRef)
  : _swapchainExtent{ windowExtent }
  , _dotDevice{ dotDeviceRef } {
    create_swapchain(_swapchainExtent);
    init_allocator();
    create_image_views();
    init_commands();
    init_sync_structures();
}

DotSwapchain::~DotSwapchain() {
    destroy_swapchain();
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(_dotDevice.get_device(), _frames[i].commandPool, nullptr);

        // destroy sync objects
        vkDestroyFence(_dotDevice.get_device(), _frames[i].renderFence, nullptr);
        vkDestroySemaphore(_dotDevice.get_device(), _frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(_dotDevice.get_device(), _frames[i].swapchainSemaphore, nullptr);
    }
    vmaDestroyAllocator(_allocator);
    vkDestroyImageView(_dotDevice.get_device(), _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
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
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void DotSwapchain::init_allocator() {
    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _dotDevice.get_gpu();
    allocatorInfo.device = _dotDevice.get_device();
    allocatorInfo.instance = _dotDevice.get_instance();
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void DotSwapchain::create_image_views() {
    // draw image size will match the window
    VkExtent3D drawImageExtent = { _swapchainExtent.width, _swapchainExtent.height, 1 };

    // hardcoding the draw format to 32 bit float
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info =
      vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(
      _allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      _drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(
      vkCreateImageView(_dotDevice.get_device(), &rview_info, nullptr, &_drawImage.imageView));

    // add to deletion queues
    _mainDeletionQueue.push_function([=, this]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    });
}

void DotSwapchain::init_commands() {
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _dotDevice.get_graphics_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {

        VK_CHECK(vkCreateCommandPool(
          _dotDevice.get_device(), &commandPoolInfo, nullptr, &_frames[i].commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo =
          vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(
          _dotDevice.get_device(), &cmdAllocInfo, &_frames[i].mainCommandBuffer));
    }
}

void DotSwapchain::init_sync_structures() {
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(
          _dotDevice.get_device(), &fenceCreateInfo, nullptr, &_frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(
          _dotDevice.get_device(), &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(
          _dotDevice.get_device(), &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));
    }
}

void DotSwapchain::submit_command_buffer(VkCommandBuffer* cmd, uint32_t imageIndex) {
    FrameData currentFrame = get_current_frame_data();
    VkImage currentImage = _swapchainImages[imageIndex];

    _dotDevice.transition_image(
      *cmd, currentImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    VK_CHECK(vkEndCommandBuffer(*cmd));

    VkCommandBufferSubmitInfo cmdinfo = _dotDevice.command_buffer_submit_info(*cmd);

    VkSemaphoreSubmitInfo waitInfo = _dotDevice.semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currentFrame.swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = _dotDevice.semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame.renderSemaphore);

    VkSubmitInfo2 submit = _dotDevice.submit_info(&cmdinfo, &signalInfo, &waitInfo);
    VK_CHECK(vkQueueSubmit2(_dotDevice.get_graphics_queue(), 1, &submit, currentFrame.renderFence));
}

void DotSwapchain::resize_swapchain(VkExtent2D newExtent) {
    vkDeviceWaitIdle(_dotDevice.get_device());
    destroy_swapchain();
    create_swapchain(newExtent);
}

void DotSwapchain::destroy_swapchain() {
    // destroy swapchain resources
    vkDestroySwapchainKHR(_dotDevice.get_device(), _swapchain, nullptr);
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_dotDevice.get_device(), _swapchainImageViews[i], nullptr);
    }
}

VkResult DotSwapchain::acquire_next_image(uint32_t* imageIndex) {
    auto frameData = get_current_frame_data();
    // std::numeric_limits<uint64_t>::max()
    VK_CHECK(vkWaitForFences(_dotDevice.get_device(), 1, &frameData.renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_dotDevice.get_device(), 1, &frameData.renderFence));
    return vkAcquireNextImageKHR(_dotDevice.get_device(),
                                 _swapchain,
                                 UINT64_MAX,
                                 frameData.swapchainSemaphore,
                                 nullptr,
                                 imageIndex);
}

VkResult DotSwapchain::present(VkQueue queue, uint32_t imageIndex) {
    auto frameData = get_current_frame_data();
    VkSemaphore waitSemaphore = frameData.renderSemaphore;
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(queue, &presentInfo);
}

}
