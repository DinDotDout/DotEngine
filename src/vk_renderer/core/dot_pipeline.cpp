#include "dot_pipeline.hpp"
#include "vk_renderer/utils/dot_initializers.hpp"
#include "vk_renderer/vk_types.hpp"
#include <iostream>
#include <vulkan/vulkan_core.h>
namespace dt {
DotPipeline::DotPipeline(std::variant<GraphicsPipelineConfig, ComputePipelineConfig> pipelineData,
                         const DotDevice& device)
  : _pipelineData(pipelineData)
  , _device{ device } {
    if (std::holds_alternative<GraphicsPipelineConfig>(_pipelineData)) {
        auto& graphicsData = std::get<GraphicsPipelineConfig>(_pipelineData);
        _init_graphics_pipeline(graphicsData);

        // Do something with graphicsData...
    } else if (std::holds_alternative<ComputePipelineConfig>(_pipelineData)) {
        auto& computeData = std::get<ComputePipelineConfig>(_pipelineData);
        _init_compute_pipeline(computeData);
        // Do something with computeData...
    }
}

DotPipeline::~DotPipeline() {
    vkDestroyPipelineLayout(_device.get_device(), _pipelineLayout, nullptr);
    vkDestroyPipeline(_device.get_device(), _pipeline, nullptr);
}

void DotPipeline::_init_compute_pipeline(ComputePipelineConfig pipelineData) {
    VkDevice device = _device.get_device();
    VkPipelineLayoutCreateInfo computeLayout = vkinit::pipeline_layout_create_info();
    computeLayout.pSetLayouts = &pipelineData.descriptorSetLayout;
    computeLayout.setLayoutCount = 1; // Should be set according to the number of sets
    if (pipelineData.pushConstantRange) {
        computeLayout.pPushConstantRanges = pipelineData.pushConstantRange;
        computeLayout.pushConstantRangeCount = 1;
    }

    VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &_pipelineLayout));

    VkShaderModule computeShader;
    if (!vkinit::load_shader_module(pipelineData.computeShaderFile, device, &computeShader)) {
        PRINT_D("Error when building the compute shader {}: ", pipelineData.computeShaderFile);
        // fmt::println("Error when building the compute shader");
    } else {
        PRINT_D("Compute shader successfully loaded {}: ", pipelineData.computeShaderFile);
        // fmt::println("Compute shader successfully loaded");
    }

    VkPipelineShaderStageCreateInfo stageinfo =
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _pipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;
    PRINT_D("Creating compute pipeline");
    VK_CHECK(vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_pipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);
}

void DotPipeline::_init_graphics_pipeline(GraphicsPipelineConfig pipelineData) {
    VkDevice device = _device.get_device();
    VkShaderModule vertexShader;
    if (!vkinit::load_shader_module(pipelineData.vertexShaderFile, device, &vertexShader)) {
        PRINT_D("Error when building the fragment shader {}: ", pipelineData.vertexShaderFile);
    } else {
        PRINT_D("Vertex shader successfully loaded {}: ", pipelineData.vertexShaderFile);
    }

    VkShaderModule fragShader;
    if (!vkinit::load_shader_module(pipelineData.fragmentShaderFile, device, &fragShader)) {
        PRINT_D("Error when building the fragment shader {}: ", pipelineData.fragmentShaderFile);
    } else {
        PRINT_D("Fragment shader successfully loaded {}: ", pipelineData.fragmentShaderFile);
    }

    VkPipelineLayoutCreateInfo graphicsLayout = vkinit::pipeline_layout_create_info();
    if (pipelineData.descriptorSetLayout) {
        graphicsLayout.setLayoutCount = 1; // TODO: Should be set according to the number of sets
        graphicsLayout.pSetLayouts = &pipelineData.descriptorSetLayout;
    }
    if (pipelineData.pushConstantRange) {
        graphicsLayout.pPushConstantRanges = pipelineData.pushConstantRange;
        graphicsLayout.pushConstantRangeCount = 1;
    }
    VK_CHECK(vkCreatePipelineLayout(device, &graphicsLayout, nullptr, &_pipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder._pipelineLayout = _pipelineLayout;

    _pipeline = pipelineBuilder.set_shaders(vertexShader, fragShader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                  .set_multisampling_none()
                  .enable_blending_additive()
                  // .enable_blending_alphablend()
                  // .disable_blending()
                  // .disable_depthtest()
                  .enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                  .set_color_attachment_format(pipelineData.imageFormat)
                  .set_depth_format(pipelineData.depthFormat)
                  .build_pipeline(device);

    vkDestroyShaderModule(device, fragShader, nullptr);
    vkDestroyShaderModule(device, vertexShader, nullptr);
}

// BUILDER IMPLEMENTATION
void DotPipeline::PipelineBuilder::clear() {
    // clear all of the structs we need back to 0 with their correct stype
    _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    _colorBlendAttachment = {};
    _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    _pipelineLayout = {};
    _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    _shaderStages.clear();
}

VkPipeline DotPipeline::PipelineBuilder::build_pipeline(VkDevice device) {
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &_colorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &_renderInfo;

    pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(
          device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        PRINT_D("Failed to create pipeline")
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    } else {
        // PRINT_D("Pipeline created cool")
        return newPipeline;
    }
}


DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_pipeline_layout(VkPipelineLayout layout){
    _pipelineLayout = layout;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_shaders(
  VkShaderModule vertexShader,
  VkShaderModule fragmentShader) {
    _shaderStages.clear();
    _shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

    _shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_input_topology(
  VkPrimitiveTopology topology) {
    _inputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    _inputAssembly.primitiveRestartEnable = VK_FALSE;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
    _rasterizer.polygonMode = mode;
    _rasterizer.lineWidth = 1.f;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode,
                                                                         VkFrontFace frontFace) {
    _rasterizer.cullMode = cullMode;
    _rasterizer.frontFace = frontFace;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_multisampling_none() {
    _multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    _multisampling.minSampleShading = 1.0f;
    _multisampling.pSampleMask = nullptr;
    // no alpha to coverage either
    _multisampling.alphaToCoverageEnable = VK_FALSE;
    _multisampling.alphaToOneEnable = VK_FALSE;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::disable_blending() {
    // default write mask
    _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // no blending
    _colorBlendAttachment.blendEnable = VK_FALSE;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::enable_blending_additive() {
    _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _colorBlendAttachment.blendEnable = VK_TRUE;
    _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::enable_blending_alphablend() {
    _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _colorBlendAttachment.blendEnable = VK_TRUE;
    _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_color_attachment_format(
  VkFormat format) {
    _colorAttachmentformat = format;
    // connect the format to the renderInfo  structure
    _renderInfo.colorAttachmentCount = 1;
    _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::set_depth_format(VkFormat format) {
    _renderInfo.depthAttachmentFormat = format;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::disable_depthtest() {
    _depthStencil.depthTestEnable = VK_FALSE;
    _depthStencil.depthWriteEnable = VK_FALSE;
    _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.stencilTestEnable = VK_FALSE;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.f;
    _depthStencil.maxDepthBounds = 1.f;
    return *this;
}

DotPipeline::PipelineBuilder& DotPipeline::PipelineBuilder::enable_depthtest(bool depthWriteEnable,
                                                                            VkCompareOp op) {
    _depthStencil.depthTestEnable = VK_TRUE;
    _depthStencil.depthWriteEnable = depthWriteEnable;
    _depthStencil.depthCompareOp = op;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.stencilTestEnable = VK_FALSE;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.f;
    _depthStencil.maxDepthBounds = 1.f;
    return *this;
}

}
