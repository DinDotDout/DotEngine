#pragma once

#include <dot_window.hpp>
// #include <vulkan/vulkan_core.h>

namespace dt {

class DotDevice {
  public:
    DotDevice(DotWindow& dotWindow);
    DotDevice(DotDevice&&) = default;
    DotDevice(const DotDevice&) = default;

    DotDevice& operator=(DotDevice&&) = delete;
    DotDevice& operator=(const DotDevice&) = delete;
    ~DotDevice();

    VkPhysicalDevice get_gpu() const { return _chosenGPU; }
    VkSurfaceKHR get_surface() const { return _surface; }
    VkDevice get_device() const { return _device; }

  private:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool _useValidationLayers = true;
#endif

    DotWindow& _dotWindow;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    VkSurfaceKHR _surface;
    VkPhysicalDevice _chosenGPU;
    VkDevice _device;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    void _init_vulkan();
    void _init_commands();
};
}
