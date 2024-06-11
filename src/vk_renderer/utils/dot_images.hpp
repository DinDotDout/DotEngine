#pragma once

#include "vk_renderer/vk_types.hpp"
#include <vulkan/vulkan.h>

namespace vkutil {

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

void transition_image(VkCommandBuffer cmd,
                      VkImage image,
                      VkImageLayout currentLayout,
                      VkImageLayout newLayout);

void copy_image_to_image(VkCommandBuffer cmd,
                         VkImage source,
                         VkImage destination,
                         VkExtent2D srcSize,
                         VkExtent2D dstSize);
} // namespace vkutil
