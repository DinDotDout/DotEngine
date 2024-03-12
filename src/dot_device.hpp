#pragma once
#include <dot_window.hpp>

#include "VkBootstrap.h"
namespace dt {

class DotDevice {

  public:
    DotDevice(DotWindow& dotWindow);
    DotDevice(const DotDevice&) = delete;
    DotDevice& operator=(const DotDevice&) = delete;
    ~DotDevice();

    VkInstance get_instance() const { return _instance; }
    VkPhysicalDevice get_gpu() const { return _chosenGPU; }
    VkSurfaceKHR get_surface() const { return _surface; }
    VkDevice get_device() const { return _device; }
    VkQueue get_graphics_queue() const { return _graphicsQueue; }
    uint32_t get_graphics_queue_family() const { return _graphicsQueueFamily; }

    VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

    void transition_image(VkCommandBuffer cmd,
                          VkImage image,
                          VkImageLayout currentLayout,
                          VkImageLayout newLayout);

    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

    VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd,
                              VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                              VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
    VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask,
                                                VkSemaphore semaphore);

  private:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool _useValidationLayers = true;
#endif

    DotWindow& _dotWindow;
    VkSurfaceKHR _surface;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;

    VkDevice _device;
    VkPhysicalDevice _chosenGPU;
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    void _init_vulkan();

    // Builder functions
    vkb::Instance _create_instance();
    void _create_surface(vkb::Instance& vkb_inst);
    vkb::PhysicalDevice _select_physical_device(vkb::Instance& vkb_inst);
    void _create_final_device(vkb::PhysicalDevice& physicalDevice);
};
}
