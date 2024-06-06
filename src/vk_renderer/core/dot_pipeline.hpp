#pragma once

#include "vk_renderer/core/dot_device.hpp"
#include <vulkan/vulkan_core.h>

#include <span>
namespace dt {
class DotPipeline {
  public:
    // DotPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, uint32_t maxSets =
    // 1000);
    DotPipeline(const DotDevice& device, const VkImageView drawImageView);
    DotPipeline(DotPipeline&&) = default;
    DotPipeline(const DotPipeline&) = delete;
    DotPipeline& operator=(DotPipeline&&) = delete;
    DotPipeline& operator=(const DotPipeline&) = delete;
    ~DotPipeline();

    VkPipeline get_gradient_pipeline() const { return _gradientPipeline; }
    VkPipelineLayout get_gradient_pipeline_layout() const { return _gradientPipelineLayout; }
    VkDescriptorSet get_descriptor_set() const { return _drawImageDescriptors; }

    struct DescriptorLayoutBuilder {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        void add_binding(uint32_t binding, VkDescriptorType type);
        void clear();
        VkDescriptorSetLayout build(VkDevice device,
                                    VkShaderStageFlags shaderStages,
                                    void* pNext = nullptr,
                                    VkDescriptorSetLayoutCreateFlags flags = 0);
    };

    struct DescriptorAllocator {
        struct PoolSizeRatio {
            VkDescriptorType type;
            float ratio;
        };

        VkDescriptorPool pool;
        void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
        void clear_descriptors(VkDevice device);
        void destroy_pool(VkDevice device);

        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
    };
    DescriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // NOTE: For now make single layout here
    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;
    void bind_descriptor_set(VkCommandBuffer cmd);

  private:
    void _init_descriptors();
    void _init_pipelines();
    void _init_background_pipelines();
    const DotDevice& _device;         // NOTE: May only need for creation
    // const DotDevice& _device;         // NOTE: May only need for creation
    const VkImageView _drawImageView; // NOTE: May only need for creation
    // const DotDevice& _device;
};
}
