#include "vk_renderer/core/dot_renderer.hpp"
#include "SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "vk_renderer/core/dot_pipeline.hpp"
#include "vk_renderer/utils/dot_images.hpp"
#include "vk_renderer/utils/dot_initializers.hpp"
#include <vulkan/vulkan_core.h>
namespace dt {
using std::make_unique;

DotRenderer::DotRenderer(DotWindow& dotWindow)
  : _dotWindow{ dotWindow } {
    _init_vulkan();
}

DotRenderer::~DotRenderer() {
    _deletionQueue.flush();
}

void DotRenderer::_init_vulkan() {
    vkb::Instance vkb_instance = _create_instance();
    _create_surface();
    _init_device(vkb_instance);
    _init_swapchain();
    _init_allocator();
    _create_draw_image_and_view();
    _init_pipeline();
    _init_imgui();
}

vkb::Instance DotRenderer::_create_instance() {
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("DotEngine")
                      .request_validation_layers(_useValidationLayers)
#ifndef NDEBUG
                      .use_default_debug_messenger()
#endif
                      .require_api_version(1, 3, 0)
                      .build();
    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _deletionQueue.push_function([=, this]() { vkDestroyInstance(_instance, nullptr); });

#ifndef NDEBUG
    _debugMessenger = vkb_inst.debug_messenger;
    _deletionQueue.push_function(
      [=, this]() { vkb::destroy_debug_utils_messenger(_instance, _debugMessenger, nullptr); });
#endif

    return vkb_inst;
}

bool DotRenderer::_create_surface() {
    if (SDL_Vulkan_CreateSurface(_dotWindow.get_window(), _instance, &_surface) == SDL_FALSE) {
        return false;
    }
    _deletionQueue.push_function(
      [=, this]() { vkDestroySurfaceKHR(_instance, _surface, nullptr); });
    return true;
}

void DotRenderer::_init_device(vkb::Instance vkb_inst) {
    _dotDevice = std::make_unique<DotDevice>(vkb_inst, _surface);
    _deletionQueue.push_function([=, this]() { _dotDevice.reset(); });
}

void DotRenderer::_init_swapchain() {
    _dotSwapchain = std::make_unique<DotSwapchain>(_dotWindow, *_dotDevice, _surface);
    _deletionQueue.push_function([=, this]() { _dotSwapchain.reset(); });
}

void DotRenderer::_init_allocator() {
    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _dotDevice->get_gpu();
    allocatorInfo.device = _dotDevice->get_device();
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
    _deletionQueue.push_function([=, this]() { vmaDestroyAllocator(_allocator); });
}

void DotRenderer::_create_draw_image_and_view() {

    VkExtent2D windowExtent = _dotWindow.get_window_extent();
    VkExtent3D drawImageExtent = { _dotWindow.get_window_extent().width,
                                   _dotWindow.get_window_extent().height,
                                   1 };

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

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(
      _allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      _drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(
      vkCreateImageView(_dotDevice->get_device(), &rview_info, nullptr, &_drawImage.imageView));

    _deletionQueue.push_function([=, this]() {
        vkDestroyImageView(_dotDevice->get_device(), _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    });
}

void DotRenderer::_init_pipeline() {
    _dotPipeline = std::make_unique<DotPipeline>(*_dotDevice, _drawImage);
    _deletionQueue.push_function([=, this]() { _dotPipeline.reset(); });
}

void DotRenderer::_init_imgui() {
    _dotImGui = make_unique<DotImGui>(
      *_dotDevice, _dotWindow.get_window(), _dotSwapchain->get_swapchain_image_format(), _instance);
    // *_dotDevice, _dotWindow.get_window(), _drawImage.imageFormat, _instance);
    _deletionQueue.push_function([=, this]() { _dotImGui.reset(); });
}

void DotRenderer::render_pass() {
    VkImage swapchainImage;
    VkImageView swapchainImageView;
    bool success = _dotSwapchain->acquire_next_image(&swapchainImage, &swapchainImageView);
    if (!success) {
        return;
    }

    auto frameData = _dotSwapchain->get_current_frame_data();
    frameData.deletionQueue.flush();

    VkCommandBuffer cmd = frameData.mainCommandBuffer;
    _begin_command_buffer(&cmd);

    /* Record command buffer */ {
        // Prepare image for rendering
        vkutil::transition_image(
          cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        _drawExtent.width = _drawImage.imageExtent.width;
        _drawExtent.height = _drawImage.imageExtent.height;

        draw_background(cmd);
        vkutil::transition_image(
          cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        draw_geometry(cmd);

        // transtion the draw image and the swapchain image into their correct transfer layouts
        vkutil::transition_image(cmd,
                                 _drawImage.image,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        // vkutil::transition_image(
        //   cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkutil::transition_image(
          cmd, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkutil::copy_image_to_image(cmd,
                                    _drawImage.image,
                                    swapchainImage,
                                    _drawExtent,
                                    _dotSwapchain->get_swapchain_extent());

        vkutil::transition_image(cmd,
                                 swapchainImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        draw_imgui(cmd, swapchainImageView);
        // Prepare for presentation
        vkutil::transition_image(cmd,
                                 swapchainImage,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    _end_command_buffer(&cmd);
    _dotSwapchain->submit_command_buffer(&cmd);
    if (!_dotSwapchain->swapchain_present_image(_dotDevice->get_graphics_queue())) {
        // std::cout << "requested in ren end \n";
    }
    _dotSwapchain->next_frame();
}

// Maybe move to own class
void DotRenderer::_end_command_buffer(VkCommandBuffer* cmd) {
    VK_CHECK(vkEndCommandBuffer(*cmd));
}

void DotRenderer::_begin_command_buffer(VkCommandBuffer* cmd) {
    VK_CHECK(vkResetCommandBuffer(*cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo =
      vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(*cmd, &cmdBeginInfo));
}

void DotRenderer::draw_background(VkCommandBuffer cmd) {
    // VkClearColorValue clearValue;
    // float flash = abs(sin(_dotSwapchain->get_current_frame() / 120.f));
    // clearValue = { { 0.0f, 0.0f, flash, 1.0f } };
    // VkImageSubresourceRange clearRange =
    // vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT); vkCmdClearColorImage(
    //   cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    const DotPipeline::ComputeEffect& effect =
      _dotPipeline->get_background_effects()[_dotPipeline->get_current_background_effect()];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
    _dotPipeline->bind_descriptor_set(cmd);
    vkCmdPushConstants(cmd,
                       effect.layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(DotPipeline::ComputePushConstants),
                       &effect.data);
    // DotPipeline::ComputePushConstants pc;
    // pc.data1 = glm::vec4(1, 0, 0, 1);
    // pc.data2 = glm::vec4(0, 0, 1, 1);
    // vkCmdPushConstants(cmd,
    //                    _dotPipeline->get_gradient_pipeline_layout(),
    //                    VK_SHADER_STAGE_COMPUTE_BIT,
    //                    0,
    //                    sizeof(DotPipeline::ComputePushConstants),
    //                    &pc);

    int32_t width_group_size = std::ceil(_drawImage.imageExtent.width / 16.0);
    uint32_t height_group_size = std::ceil(_drawImage.imageExtent.height / 16.0);
    vkCmdDispatch(cmd, width_group_size, height_group_size, 1);
}

void DotRenderer::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment =
      vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
      vkinit::rendering_info(_dotSwapchain->get_swapchain_extent(), &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void DotRenderer::draw_geometry(VkCommandBuffer cmd) {
    // begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      _drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _dotPipeline->get_triangle_pipeline());

    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    // launch a draw command to draw 3 vertices
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

void DotRenderer::resize_swapchain() {
    _dotSwapchain->resize_swapchain(_dotWindow.get_window_extent());
}

void DotRenderer::wait_device_idle() {
    vkDeviceWaitIdle(_dotDevice->get_device());
}

const std::vector<DotPipeline::ComputeEffect>& DotRenderer::get_background_effects() {
    return _dotPipeline->get_background_effects();
}

int DotRenderer::get_current_background_effect() {
    return _dotPipeline->get_current_background_effect();
}

} // namespace dt
