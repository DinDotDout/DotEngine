#pragma once

#include <variant>
#include <vulkan/vulkan_core.h>

// #include "vk_renderer/core/dot_descriptors.hpp"
#include "vk_renderer/core/dot_device.hpp"
// #include "vk_renderer/vk_types.hpp"

namespace dt {

class DotPipeline {
    // TODO: Build pipeline struct with shader/effects on it.
  public:
    // TODO: std::vector<VkDescriptorSetLayout> descriptorSetLayouts and fetch with size() and
    // data()?; NOTE: Should create a wrapper for matterials keeping name, pipeline and data?
    struct GraphicsPipelineConfig {
        const char* vertexShaderFile = nullptr;
        const char* fragmentShaderFile = nullptr;
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkPushConstantRange* pushConstantRange;
        VkDescriptorSetLayout descriptorSetLayout = nullptr;
        // VkDescriptorSetLayout descriptorSetLayout;
    };

    struct ComputePipelineConfig {
        const char* computeShaderFile = nullptr;
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        VkPushConstantRange* pushConstantRange;
        // VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSetLayout descriptorSetLayout = nullptr;
        // PushConstants data;
    };

    DotPipeline(std::variant<GraphicsPipelineConfig, ComputePipelineConfig> pipelineData,
                const DotDevice& device);

    DotPipeline(DotPipeline&&) = default;
    DotPipeline(const DotPipeline&) = delete;
    DotPipeline& operator=(DotPipeline&&) = delete;
    DotPipeline& operator=(const DotPipeline&) = delete;
    ~DotPipeline();

    class PipelineBuilder {
      public:
        std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
        VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
        VkPipelineRasterizationStateCreateInfo _rasterizer;
        VkPipelineColorBlendAttachmentState _colorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo _multisampling;
        VkPipelineLayout _pipelineLayout;
        VkPipelineDepthStencilStateCreateInfo _depthStencil;
        VkPipelineRenderingCreateInfo _renderInfo;
        VkFormat _colorAttachmentformat;

        PipelineBuilder() {
            clear();
        }

        void clear();
        VkPipeline build_pipeline(VkDevice device);

        PipelineBuilder& set_pipeline_layout(VkPipelineLayout layout);
        PipelineBuilder& set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
        PipelineBuilder& set_input_topology(VkPrimitiveTopology topology);
        PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
        PipelineBuilder& set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
        PipelineBuilder& set_multisampling_none();
        PipelineBuilder& disable_blending();
        PipelineBuilder& enable_blending_additive();
        PipelineBuilder& enable_blending_alphablend();
        PipelineBuilder& set_color_attachment_format(VkFormat format);
        PipelineBuilder& set_depth_format(VkFormat format);
        PipelineBuilder& disable_depthtest();
        PipelineBuilder& enable_depthtest(bool depthWriteEnable, VkCompareOp op);
    };

    VkPipeline get_pipeline() const { return _pipeline; }
    VkPipelineLayout get_pipeline_layout() const { return _pipelineLayout; }
    // const char* get_name() const { return _name; }
    // Onlye keep this around in this class, rest should be out.

    const DotDevice& _device; // NOTE: May only need for creation and deleted.
    // const char* _name;
    VkPipeline _pipeline;
    VkPipelineLayout _pipelineLayout;
    std::variant<GraphicsPipelineConfig, ComputePipelineConfig> _pipelineData;

  private:
    void _init_graphics_pipeline(GraphicsPipelineConfig pipelineData);
    void _init_compute_pipeline(ComputePipelineConfig pipelineData);
};
}
