#include "vk_renderer/core/dot_swapchain.hpp"

#include <VkBootstrap.h>

#include <dot_window.hpp>
#include <vk_renderer/core/dot_device.hpp>
#include <vk_renderer/utils/dot_initializers.hpp>
#include <vk_renderer/vk_types.hpp>

namespace dt {

DotSwapchain::DotSwapchain(const DotWindow& dotWindow,
                           const DotDevice& dotDevice,
                           const VkSurfaceKHR& surface)
  // DotSwapchain::DotSwapchain(VkExtent2D windowExtent, const DotDevice& dotDeviceRef)
  : _dotDevice{ dotDevice }
  , _dotWindow{ dotWindow }
  , _surface{ surface }
  , _swapchainExtent{ dotWindow.get_window_extent() } {
    create_swapchain(_swapchainExtent);
    create_frame_command_pools();
    allocate_frame_command_buffers();
    init_descriptor_pools();
    create_frame_sync_structures();
    create_immediate_submit_structures();
}

DotSwapchain::~DotSwapchain() {
    // NOTE: Refactor with RAII
    _deletionQueue.flush();
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(_dotDevice.get_device(), _frames[i].commandPool, nullptr);
        vkDestroyFence(_dotDevice.get_device(), _frames[i].renderFence, nullptr);
        vkDestroySemaphore(_dotDevice.get_device(), _frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(_dotDevice.get_device(), _frames[i].swapchainSemaphore, nullptr);
        _frames[i].deletionQueue.flush();
    }
    destroy_swapchain();
}

void DotSwapchain::clear_frames() {
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _frames[i].deletionQueue.flush();
    }
}

void DotSwapchain::create_swapchain(VkExtent2D swapchainExtent) {
    vkb::SwapchainBuilder swapchainBuilder{ _dotDevice.get_gpu(),
                                            _dotDevice.get_device(),
                                            _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
      swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat,
                                                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        // use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        // .set_desired_extent(extent.width, extent.height)
        .set_desired_extent(swapchainExtent.width, swapchainExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    // vkb created all of this for us
    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

// Could maybe merge into single function "prepare" frame data
void DotSwapchain::create_frame_command_pools() {
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _dotDevice.get_graphics_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(
          _dotDevice.get_device(), &commandPoolInfo, nullptr, &_frames[i].commandPool));
    }
}

void DotSwapchain::allocate_frame_command_buffers() {
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkCommandBufferAllocateInfo cmdAllocInfo =
          vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(
          _dotDevice.get_device(), &cmdAllocInfo, &_frames[i].mainCommandBuffer));
    }
}

void DotSwapchain::create_frame_sync_structures() {
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
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

void DotSwapchain::init_descriptor_pools() {
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i].frameDescriptors.init(_dotDevice.get_device(), 1000, frame_sizes);

        _deletionQueue.push_function(
          [&, i]() { _frames[i].frameDescriptors.destroy_pools(_dotDevice.get_device()); });
    }
}

void DotSwapchain::create_immediate_submit_structures() {
    VkDevice device = _dotDevice.get_device();
    {
        VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
          _dotDevice.get_graphics_queue_family(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &_immediateCommandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo =
          vkinit::command_buffer_allocate_info(_immediateCommandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &_immediateCommandBuffer));
        _deletionQueue.push_function(
          [=, this]() { vkDestroyCommandPool(device, _immediateCommandPool, nullptr); });
    }
    {
        VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &_immediateFence));
        _deletionQueue.push_function(
          [=, this]() { vkDestroyFence(device, _immediateFence, nullptr); });
    }
}

void DotSwapchain::resize_swapchain(VkExtent2D newExtent) {
    vkDeviceWaitIdle(_dotDevice.get_device());
    destroy_swapchain();
    create_swapchain(newExtent);
}

void DotSwapchain::destroy_swapchain() {
    vkDestroySwapchainKHR(_dotDevice.get_device(), _swapchain, nullptr);
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_dotDevice.get_device(), _swapchainImageViews[i], nullptr);
    }
}

bool DotSwapchain::acquire_next_image(VkImage* image, VkImageView* imageView) {
    FrameData& frameData = get_current_frame_data();
    VkDevice device = _dotDevice.get_device();
    // std::numeric_limits<uint64_t>::max()
    VK_CHECK(vkWaitForFences(device, 1, &frameData.renderFence, true, 1000000000));
    // TODO: Move frameData clearing to resource manager
    frameData.deletionQueue.flush();

    frameData.frameDescriptors.clear_pools(device);
    VkResult result = vkAcquireNextImageKHR(
      device, _swapchain, UINT64_MAX, frameData.swapchainSemaphore, nullptr, &_swapchainImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_swapchain(_dotWindow.get_window_extent());
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire next swapchain image!");
        return false;
    }
    VK_CHECK(vkResetFences(device, 1, &frameData.renderFence));
    *image = _swapchainImages[_swapchainImageIndex];
    *imageView = _swapchainImageViews[_swapchainImageIndex];
    return true;
}

bool DotSwapchain::swapchain_present_image(VkQueue queue) {
    FrameData& frameData = get_current_frame_data();
    VkSemaphore waitSemaphore = frameData.renderSemaphore;
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &_swapchainImageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_swapchain(_dotWindow.get_window_extent());
        PRINT_D("resizing");
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        PRINT_D("throwing");
        throw std::runtime_error("Failed to acquire next swapchain image!");
    }
    _frame++;
    return true;
}

void DotSwapchain::submit_command_buffer(VkCommandBuffer cmd) {
    FrameData& frameData = get_current_frame_data();
    VkImage currentImage = _swapchainImages[_swapchainImageIndex];

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frameData.swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frameData.renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);
    VK_CHECK(vkQueueSubmit2(_dotDevice.get_graphics_queue(), 1, &submit, frameData.renderFence));
}

void DotSwapchain::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VkDevice device = _dotDevice.get_device();
    VK_CHECK(vkResetFences(device, 1, &_immediateFence));
    VK_CHECK(vkResetCommandBuffer(_immediateCommandBuffer, 0));

    VkCommandBuffer cmd = _immediateCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo =
      vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_dotDevice.get_graphics_queue(), 1, &submit, _immediateFence));
    VK_CHECK(vkWaitForFences(device, 1, &_immediateFence, true, 9999999999));
}

} // namespace dt
