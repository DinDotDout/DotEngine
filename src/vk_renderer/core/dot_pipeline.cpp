#include "dot_pipeline.hpp"
#include "vk_renderer/utils/dot_initializers.hpp"
#include "vk_renderer/vk_types.hpp"
namespace dt {
DotPipeline::DotPipeline(const DotDevice& device, const VkImageView drawImageView)
  : _device{ device }
  , _drawImageView{ drawImageView } {
    _init_descriptors();
    _init_pipelines();
}

DotPipeline::~DotPipeline() {
    vkDestroyDescriptorSetLayout(_device.get_device(), _drawImageDescriptorLayout, nullptr);
    globalDescriptorAllocator.clear_descriptors(_device.get_device());
    globalDescriptorAllocator.destroy_pool(_device.get_device());
    vkDestroyPipelineLayout(_device.get_device(), _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device.get_device(), _gradientPipeline, nullptr);
}

void DotPipeline::DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DotPipeline::DescriptorLayoutBuilder::clear() {
    bindings.clear();
}

VkDescriptorSetLayout DotPipeline::DescriptorLayoutBuilder::build(
  VkDevice device,
  VkShaderStageFlags shaderStages,
  void* pNext,
  VkDescriptorSetLayoutCreateFlags flags) {
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    };
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
    return set;
}

void DotPipeline::DescriptorAllocator::init_pool(VkDevice device,
                                                 uint32_t maxSets,
                                                 std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
          .type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * maxSets) });
    }

    VkDescriptorPoolCreateInfo pool_info = { .sType =
                                               VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DotPipeline::DescriptorAllocator::clear_descriptors(VkDevice device) {
    vkResetDescriptorPool(device, pool, 0);
}

void DotPipeline::DescriptorAllocator::destroy_pool(VkDevice device) {
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DotPipeline::DescriptorAllocator::allocate(VkDevice device,
                                                           VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo = { .sType =
                                                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

void DotPipeline::_init_descriptors() {
    // create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DotPipeline::DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    VkDevice device = _device.get_device();
    globalDescriptorAllocator.init_pool(device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors = globalDescriptorAllocator.allocate(device, _drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);
}

void DotPipeline::_init_background_pipelines() {
    VkDevice device = _device.get_device();
    // Layout
    // VkPipelineLayoutCreateInfo computeLayout = vkinit::pipeline_layout_create_info();
    // computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    // Same as prev line
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &_gradientPipelineLayout));

    // Object creation
    VkShaderModule computeDrawShader;
    if (!vkinit::load_shader_module(
          "built_shaders/gradient.comp.spv", device, &computeDrawShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));
    vkDestroyShaderModule(device, computeDrawShader, nullptr);
}

void DotPipeline::_init_pipelines() {
    _init_background_pipelines();
}

void DotPipeline::bind_descriptor_set(VkCommandBuffer cmd) {
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            _gradientPipelineLayout,
                            0,
                            1,
                            &_drawImageDescriptors,
                            0,
                            nullptr);

    // VkDescriptorSet descriptorSet = _dotPipeline->get_descriptor_set();
}

}
