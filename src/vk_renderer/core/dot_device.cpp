#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/vk_types.hpp"
#include <cassert>
#include <iostream>
namespace dt {
using std::cout;

DotDevice::DotDevice(vkb::Instance& instance, VkSurfaceKHR& surface) {
    _init_device(instance, surface);
}

DotDevice::~DotDevice() {
    vkDestroyDevice(_device, nullptr);
}

void DotDevice::_init_device(vkb::Instance& instance, VkSurfaceKHR& surface) {
    VkPhysicalDeviceVulkan13Features features{};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{ instance };
    vkb::Result<vkb::PhysicalDevice> result = selector.set_minimum_version(1, 3)
                                                .set_required_features_13(features)
                                                .set_required_features_12(features12)
                                                .set_surface(surface)
                                                .select();
    if (!result) {
        std::cout << "Error message: " << result.error().message() << std::endl;
        return;
    }

    vkb::PhysicalDevice vkbPhysicalDevice = result.value();
    _physicalDevice = vkbPhysicalDevice.physical_device;

    vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();
    _device = vkbDevice.device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}
} // namespace dt
