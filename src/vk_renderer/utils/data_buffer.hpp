#pragma once
// #include "vk_renderer/vk_types.hpp"
#include "vk_renderer/vk_types.hpp"

namespace dt {
// TODO: Vk backend should not be in charge of loading this.
AllocatedBuffer create_buffer(VmaAllocator allocator,
                              size_t allocSize,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memoryUsage);

void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer& buffer);
}
