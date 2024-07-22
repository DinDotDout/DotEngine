#pragma once
#include "VkBootstrap.h"
namespace dt {

class DotDevice {
public:

    explicit DotDevice(vkb::Instance& instance, VkSurfaceKHR& surface);
    DotDevice(const DotDevice&) = delete;
    DotDevice& operator=(const DotDevice&) = delete;
    ~DotDevice();


    // VkInstance get_instance() const { return _instance; }
    VkPhysicalDevice get_gpu() const { return _physicalDevice; }
    VkDevice get_device() const { return _device; }
    VkQueue get_graphics_queue() const { return _graphicsQueue; }
    uint32_t get_graphics_queue_family() const { return _graphicsQueueFamily; }

  private:
    VkDevice _device;
    VkPhysicalDevice _physicalDevice;
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    void _init_device(vkb::Instance& instance, VkSurfaceKHR& surface);
};
}  // namespace dt
