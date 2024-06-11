#pragma once

#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/utils/dot_images.hpp"
#include <iostream>
#include <span>
#include <vulkan/vulkan_core.h>

// class AllocatedImage;
namespace dt {
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

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device,
                                VkShaderStageFlags shaderStages,
                                void* pNext = nullptr,
                                VkDescriptorSetLayoutCreateFlags flags = 0);
};

class DotPipeline {
    // Build pipeline struct with shader/effects on it
  public:
    // NOTE: Will need to be cleaned
    struct ComputePushConstants {
        glm::vec4 data1;
        glm::vec4 data2;
        glm::vec4 data3;
        glm::vec4 data4;
    };

    struct ComputeEffect {
        const char* name;
        VkPipeline pipeline;
        VkPipelineLayout layout;

        ComputePushConstants data;
    };
    // DotPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, uint32_t maxSets =
    // 1000);
    DotPipeline(const DotDevice& device, const vkutil::AllocatedImage& allocImage);
    DotPipeline(DotPipeline&&) = default;
    DotPipeline(const DotPipeline&) = delete;
    DotPipeline& operator=(DotPipeline&&) = delete;
    DotPipeline& operator=(const DotPipeline&) = delete;
    ~DotPipeline();
    class PipelineBuilder {
      public:
        // PipelineBuilder(const PipelineBuilder&) = default;
        // PipelineBuilder(PipelineBuilder&&) = default;
        // PipelineBuilder& operator=(const PipelineBuilder&) = default;
        // PipelineBuilder& operator=(PipelineBuilder&&) = default;
        std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

        VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
        VkPipelineRasterizationStateCreateInfo _rasterizer;
        VkPipelineColorBlendAttachmentState _colorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo _multisampling;
        VkPipelineLayout _pipelineLayout;
        VkPipelineDepthStencilStateCreateInfo _depthStencil;
        VkPipelineRenderingCreateInfo _renderInfo;
        VkFormat _colorAttachmentformat;

        PipelineBuilder() { clear(); }

        void clear();
        VkPipeline build_pipeline(VkDevice device);
        //< pipeline
        PipelineBuilder set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
        PipelineBuilder set_input_topology(VkPrimitiveTopology topology);
        PipelineBuilder set_polygon_mode(VkPolygonMode mode);
        PipelineBuilder set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
        PipelineBuilder set_multisampling_none();
        PipelineBuilder disable_blending();
        PipelineBuilder enable_blending_additive();
        PipelineBuilder enable_blending_alphablend();

        PipelineBuilder set_color_attachment_format(VkFormat format);
        PipelineBuilder set_depth_format(VkFormat format);
        PipelineBuilder disable_depthtest();
        PipelineBuilder enable_depthtest(bool depthWriteEnable, VkCompareOp op);
    };

    // NOTE: This is a bit of a mess, just store pipelines in engine class
    VkPipeline get_gradient_pipeline() const { return _gradientPipeline; }
    VkPipeline get_triangle_pipeline() const { return _trianglePipeline; }
    VkPipelineLayout get_gradient_pipeline_layout() const { return _gradientPipelineLayout; }
    VkDescriptorSet get_descriptor_set() const { return _drawImageDescriptors; }
    const std::vector<ComputeEffect>& get_background_effects() const { return backgroundEffects; }

    // Accessor for currentBackgroundEffect
    int get_current_background_effect() const { return currentBackgroundEffect; }
    void set_current_background_effect(int backgroundEffect) {
        currentBackgroundEffect = backgroundEffect;
    }

    DescriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet
      _drawImageDescriptors; // Descriptor set for the image this is going to work onto
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // NOTE: For now make single layout here
    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;
    void bind_descriptor_set(VkCommandBuffer cmd);

  private:
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{ 0 };
    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;

    // NOTE: All this should be generic and not specific to the pipeline.
    void _init_descriptors();
    void _init_pipelines();
    void _init_background_pipelines();
    void _init_triangle_pipeline();
    const DotDevice& _device; // NOTE: May only need for creation
    // const DotDevice& _device;         // NOTE: May only need for creation
    const vkutil::AllocatedImage& _allocImage;
    // const VkImage _drawImage;         // NOTE: May only need for creation
    // const VkImageView _drawImageView; // NOTE: May only need for creation
    // const DotDevice& _device;
};
}
