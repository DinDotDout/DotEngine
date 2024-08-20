#include "vk_renderer/core/vk_backend.hpp"

#include "SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include "glm/packing.hpp"

#include "dot_window.hpp"
#include "vk_renderer/core/dot_descriptors.hpp"
#include "vk_renderer/core/dot_device.hpp"
#include "vk_renderer/core/dot_pipeline.hpp"
#include "vk_renderer/core/dot_swapchain.hpp"
#include "vk_renderer/utils/data_buffer.hpp"
#include "vk_renderer/utils/dot_initializers.hpp"
#include <vk_renderer/core/dot_imgui.hpp>
#include <vulkan/vulkan_core.h>
// #include <vk_renderer/utils/dot_loader.hpp>

#define VMA_IMPLEMENTATION
// #include "vk_renderer/vk_types.hpp" // Vma allocator needed
#include <vk_mem_alloc.h>

namespace dt {

// using std::make_unique;

VkBackend::VkBackend(DotWindow& dotWindow)
  : _dotWindow{ dotWindow } {
    _init_vulkan();
}

VkBackend::~VkBackend() {
    // for (auto& mesh : testMeshes) {
    //     destroy_buffer(_allocator, mesh->meshBuffers.indexBuffer);
    //     destroy_buffer(_allocator, mesh->meshBuffers.vertexBuffer);
    // }
    // PRINT_D("ending renderer")
    loadedScenes.clear();

    // Still needed?
    _dotSwapchain->clear_frames(); // We must free images before vma itself gets deleted
    metalRoughMaterial.clear_resources(_dotDevice->get_device());
    _deletionQueue.flush();
}

void VkBackend::_init_vulkan() {
    vkb::Instance vkb_instance = _create_instance();
    _create_surface();
    // Vulkan basic stuff
    _init_device(vkb_instance);
    _init_swapchain();
    _init_allocator();

    _create_draw_image_and_view();
    _init_descriptors();
    _init_pipelines();
    _init_imgui();
    _init_default_data();
    _init_renderables();
}

vkb::Instance VkBackend::_create_instance() {
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("DotEngine")
                      .request_validation_layers(_useValidationLayers)
#ifndef NDEBUG
                      .use_default_debug_messenger()
#endif
                      .require_api_version(1, 3, 0)
                      .build();
    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting instance");
        vkDestroyInstance(_instance, nullptr);
    });

#ifndef NDEBUG
    _debugMessenger = vkb_inst.debug_messenger;
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting messenger");
        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger, nullptr);
    });
#endif

    return vkb_inst;
}

bool VkBackend::_create_surface() {
    if (SDL_Vulkan_CreateSurface(_dotWindow.get_window(), _instance, &_surface) == SDL_FALSE) {
        PRINT_D("Surface creation failed.");
        return false;
    }
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting surface");
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
    });
    return true;
}

void VkBackend::_init_device(vkb::Instance& vkb_inst) {
    _dotDevice = std::make_unique<DotDevice>(vkb_inst, _surface);
    // _deletionQueue.push_function([&]() {
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting device");
        _dotDevice.reset();
    });
}

void VkBackend::_init_swapchain() {
    _dotSwapchain = std::make_unique<DotSwapchain>(_dotWindow, *_dotDevice, _surface);
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting swapchain");
        _dotSwapchain.reset();
    });
}

void VkBackend::_init_allocator() {
    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _dotDevice->get_gpu();
    allocatorInfo.device = _dotDevice->get_device();
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting vma");
        vmaDestroyAllocator(_allocator);
    });
}

void VkBackend::_init_default_data() {
    /* init rect */ {
        std::array<Vertex, 4> rect_vertices;

        rect_vertices[0].position = { 0.5, -0.5, 0 };
        rect_vertices[1].position = { 0.5, 0.5, 0 };
        rect_vertices[2].position = { -0.5, -0.5, 0 };
        rect_vertices[3].position = { -0.5, 0.5, 0 };

        rect_vertices[0].color = { 0, 0, 0, 1 };
        rect_vertices[1].color = { 0.5, 0.5, 0.5, 1 };
        rect_vertices[2].color = { 1, 0, 0, 1 };
        rect_vertices[3].color = { 0, 1, 0, 1 };

        std::array<uint32_t, 6> rect_indices;

        rect_indices[0] = 0;
        rect_indices[1] = 1;
        rect_indices[2] = 2;

        rect_indices[3] = 2;
        rect_indices[4] = 1;
        rect_indices[5] = 3;

        rectangle = upload_mesh(rect_indices, rect_vertices);
        // delete the rectangle data on engine shutdown
        _deletionQueue.push_function([&]() {
            destroy_buffer(_allocator, rectangle.indexBuffer);
            destroy_buffer(_allocator, rectangle.vertexBuffer);
        });
    }

    /* init basic mesh */ {
        // Loads cube and suzanne
        testMeshes = load_gltf_meshes(this, "assets/basicmesh.glb").value();
        _deletionQueue.push_function([=, this]() {
            for (auto& mesh : testMeshes) {
                destroy_buffer(_allocator, mesh->meshBuffers.indexBuffer);
                destroy_buffer(_allocator, mesh->meshBuffers.vertexBuffer);
            }
        });
    }
    /* init default textures */ {
        VkDevice device = _dotDevice->get_device();

        // 3 default textures, white, grey, black. 1 pixel each
        uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
        _whiteImage = create_image(_allocator,
                                   device,
                                   VkExtent3D{ 1, 1, 1 },
                                   VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT);

        write_texture_data((void*)&white, _whiteImage);

        uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
        _greyImage = create_image(_allocator,
                                  device,
                                  VkExtent3D{ 1, 1, 1 },
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_SAMPLED_BIT);

        write_texture_data((void*)&grey, _greyImage);
        uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        _blackImage = create_image(_allocator,
                                   device,
                                   VkExtent3D{ 1, 1, 1 },
                                   VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT);
        write_texture_data((void*)&black, _blackImage);

        // checkerboard image
        uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        _errorCheckerboardImage = create_image(_allocator,
                                               device,
                                               VkExtent3D{ 16, 16, 1 },
                                               VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_USAGE_SAMPLED_BIT);

        write_texture_data(pixels.data(), _errorCheckerboardImage);

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

        sampl.magFilter = VK_FILTER_NEAREST;
        sampl.minFilter = VK_FILTER_NEAREST;

        vkCreateSampler(device, &sampl, nullptr, &_defaultSamplerNearest);

        sampl.magFilter = VK_FILTER_LINEAR;
        sampl.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(device, &sampl, nullptr, &_defaultSamplerLinear);

        _deletionQueue.push_function([&]() {
            VkDevice device = _dotDevice->get_device();
            vkDestroySampler(device, _defaultSamplerNearest, nullptr);
            vkDestroySampler(device, _defaultSamplerLinear, nullptr);
            destroy_image(_allocator, device, _whiteImage);
            destroy_image(_allocator, device, _greyImage);
            destroy_image(_allocator, device, _blackImage);
            destroy_image(_allocator, device, _errorCheckerboardImage);
        });
    }

    {
        GLTFMetallic_Roughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = _whiteImage;
        materialResources.colorSampler = _defaultSamplerLinear;
        materialResources.metalRoughImage = _whiteImage;
        materialResources.metalRoughSampler = _defaultSamplerLinear;

        // set the uniform buffer for the material data
        AllocatedBuffer materialConstants =
          create_buffer(_allocator,
                        sizeof(GLTFMetallic_Roughness::MaterialConstants),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);

        // write the buffer
        GLTFMetallic_Roughness::MaterialConstants* sceneUniformData =
          (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
        sceneUniformData->colorFactors = glm::vec4{ 1, 1, 1, 1 };
        sceneUniformData->metal_rough_factors = glm::vec4{ 1, 0.5, 0, 0 };

        _deletionQueue.push_function(
          [=, this]() { destroy_buffer(_allocator, materialConstants); });

        materialResources.dataBuffer = materialConstants.buffer;
        materialResources.dataBufferOffset = 0;

        defaultData = metalRoughMaterial.write_material(_dotDevice->get_device(),
                                                        MaterialPass::MainColor,
                                                        materialResources,
                                                        globalDescriptorAllocator);
    }
    {
        // for (auto& m : testMeshes) {
        for (const std::shared_ptr<MeshAsset>& m :
             testMeshes) { // Type: `std::vector<std::shared_ptr<MeshAsset>>`
            std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
            newNode->mesh = m;
            newNode->localTransform = glm::mat4{ 1.f };
            newNode->worldTransform = glm::mat4{ 1.f };
            for (auto& s : newNode->mesh->surfaces) {
                s.material = std::make_shared<GLTFMaterial>(defaultData);
            }

            loadedNodes[m->name] = std::move(newNode);
        }
    }
}

void VkBackend::_init_renderables() {
    std::string structurePath = { "assets/structure.glb" };
    // std::string structurePath = { "..\\..\\assets\\structure.glb" };
    auto structureFile = load_gltf(this, structurePath);

    assert(structureFile.has_value());

    loadedScenes["structure"] = *structureFile;
}

void VkBackend::_create_draw_image_and_view() {
    VkDevice device = _dotDevice->get_device();
    VkExtent2D windowExtent = _dotWindow.get_window_extent();
    VmaAllocationCreateInfo drawImageAllocInfo = {};
    drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    drawImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    /* draw image */ {
        VkExtent3D drawImageExtent = { windowExtent.width, windowExtent.height, 1 };

        // Hardcoding the draw format
        _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        _drawImage.imageExtent = drawImageExtent;

        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkImageCreateInfo drawImageInfo =
          vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

        vmaCreateImage(_allocator,
                       &drawImageInfo,
                       &drawImageAllocInfo,
                       &_drawImage.image,
                       &_drawImage.allocation,
                       nullptr);

        VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
          _drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &_drawImage.imageView));

        _deletionQueue.push_function([&]() {
            PRINT_D("Deleting draw_image");
            vkDestroyImageView(_dotDevice->get_device(), _drawImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        });
    }

    /*depth image*/ {
        _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
        _depthImage.imageExtent = _drawImage.imageExtent;
        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo depthImageInfo = vkinit::image_create_info(
          _depthImage.imageFormat, depthImageUsages, _drawImage.imageExtent);

        // allocate and create the image
        vmaCreateImage(_allocator,
                       &depthImageInfo,
                       &drawImageAllocInfo,
                       &_depthImage.image,
                       &_depthImage.allocation,
                       nullptr);

        // build a image-view for the draw image to use for rendering
        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
          _depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

        VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &_depthImage.imageView));

        _deletionQueue.push_function([&]() {
            PRINT_D("Deleting depth image");
            vkDestroyImageView(_dotDevice->get_device(), _depthImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        });
    }
}

void VkBackend::_init_descriptors() {
    VkDevice device = _dotDevice->get_device();
    // create a descriptor pool that will hold 10 sets with 1 image each
    // std::vector<DescriptorAllocator::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    //                                                             1 } };

    // globalDescriptorAllocator.init_pool(device, 10, sizes);
    //
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    globalDescriptorAllocator.init(device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors = globalDescriptorAllocator.allocate(device, _drawImageDescriptorLayout);

    // VkDescriptorImageInfo imgInfo{};
    // imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    // imgInfo.imageView = _drawImage.imageView;
    //
    // VkWriteDescriptorSet drawImageWrite = {};
    // drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // drawImageWrite.pNext = nullptr;
    //
    // drawImageWrite.dstBinding = 0;
    // drawImageWrite.dstSet = _drawImageDescriptors;
    // drawImageWrite.descriptorCount = 1;
    // drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // drawImageWrite.pImageInfo = &imgInfo;
    //
    // vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);
    DescriptorWriter writer;
    writer.write_image(0,
                       _drawImage.imageView,
                       VK_NULL_HANDLE,
                       VK_IMAGE_LAYOUT_GENERAL,
                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(device, _drawImageDescriptors);

    _deletionQueue.push_function([=, this]() {
        // vkDestroyDescriptorPool(_dotDevice->get_device(), globalDescriptorAllocator.pool,
        // nullptr);
        globalDescriptorAllocator.destroy_pools(_dotDevice->get_device());
        vkDestroyDescriptorSetLayout(_dotDevice->get_device(), _drawImageDescriptorLayout, nullptr);
    });

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout =
          builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        _deletionQueue.push_function([=, this]() {
            vkDestroyDescriptorSetLayout(
              _dotDevice->get_device(), _gpuSceneDataDescriptorLayout, nullptr);
        });
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
        _deletionQueue.push_function([=, this]() {
            vkDestroyDescriptorSetLayout(
              _dotDevice->get_device(), _singleImageDescriptorLayout, nullptr);
        });
    }
}

void VkBackend::_init_pipelines() {
    /* Sky pipeline */ {
        VkPushConstantRange bufferRange{};
        bufferRange.offset = 0;
        bufferRange.size = sizeof(PushConstants);
        bufferRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        DotPipeline::ComputePipelineConfig _sky_gradientData = {
            .computeShaderFile = "build/built_shaders/sky.comp.spv",
            .pushConstantRange = &bufferRange,
            .descriptorSetLayout = _drawImageDescriptorLayout,
        };

        PushConstants data = { .data1 = glm::vec4(0.1, 0.2, 0.4, 0.97) };
        _computeEffects.push_back(ComputeEffect{
          "stars", std::make_unique<DotPipeline>(_sky_gradientData, *_dotDevice), data });

        _deletionQueue.push_function([=, this]() {
            PRINT_D("Deleting sky pipeline.");
            _computeEffects[0].pipeline.reset();
        });
    }

    /* Gradient pipeline */ {
        VkPushConstantRange bufferRange{};
        bufferRange.offset = 0;
        bufferRange.size = sizeof(PushConstants);
        bufferRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        DotPipeline::ComputePipelineConfig _bg_gradientData = {
            .computeShaderFile = "build/built_shaders/gradient_color.comp.spv",
            .pushConstantRange = &bufferRange,
            .descriptorSetLayout = _drawImageDescriptorLayout,
        };
        PushConstants data = { .data1 = glm::vec4(1, 0, 0, 1), .data2 = glm::vec4(0, 0, 1, 1) };
        _computeEffects.push_back(ComputeEffect{
          "gradient", std::make_unique<DotPipeline>(_bg_gradientData, *_dotDevice), data });
        _deletionQueue.push_function([=, this]() {
            PRINT_D("Deleting gradient pipeline.");
            _computeEffects[1].pipeline.reset();
        });
    }

    // /* triangle pipeline */ {
    //     DotPipeline::GraphicsPipelineConfig trianglePipelineData = {
    //         .vertexShaderFile = "built_shaders/colored_triangle.vert.spv",
    //         .fragmentShaderFile = "built_shaders/colored_triangle.frag.spv",
    //         .imageFormat = _drawImage.imageFormat
    //     };
    //     _trianglePipeline = std::make_unique<DotPipeline>(trianglePipelineData, *_dotDevice);
    //     _deletionQueue.push_function([=, this]() {
    //         PRINT_D("Deleting triangle pipeline.");
    //         _trianglePipeline.reset();
    //     });
    // }

    /* mesh pipeline */ {
        VkPushConstantRange bufferRange{};
        // VkPushConstantRange bufferRange{};
        bufferRange.offset = 0;
        bufferRange.size = sizeof(GPUDrawPushConstants);
        bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        DotPipeline::GraphicsPipelineConfig meshPipelineData = {
            .vertexShaderFile = "build/built_shaders/colored_triangle_mesh.vert.spv",
            // .fragmentShaderFile = "built_shaders/colored_triangle.frag.spv",
            .fragmentShaderFile = "build/built_shaders/tex_image.frag.spv",
            .imageFormat = _drawImage.imageFormat,
            .depthFormat = _depthImage.imageFormat,
            .pushConstantRange = &bufferRange,
            .descriptorSetLayout = _singleImageDescriptorLayout,
        };
        // DotPipeline::GraphicsPipelineData _meshPipeline = {};
        _meshPipeline = std::make_unique<DotPipeline>(meshPipelineData, *_dotDevice);
        _deletionQueue.push_function([=, this]() {
            PRINT_D("Deleting mesh pipeline.");
            _meshPipeline.reset();
        });
    }

    metalRoughMaterial.build_pipelines(this);
    // _deletionQueue.push_function(
    //   [=, this]() { metalRoughMaterial.clear_resources(_dotDevice->get_device()); });
}

void VkBackend::_init_imgui() {
    _dotImGui = std::make_unique<DotImGui>(
      *_dotDevice, _dotWindow.get_window(), _dotSwapchain->get_swapchain_image_format(), _instance);
    // *_dotDevice, _dotWindow.get_window(), _drawImage.imageFormat, _instance);
    _deletionQueue.push_function([=, this]() {
        PRINT_D("Deleting imgui");
        _dotImGui.reset();
    });
}

void VkBackend::render() {
    VkImage swapchainImage;
    VkImageView swapchainImageView;
    bool success = _dotSwapchain->acquire_next_image(&swapchainImage, &swapchainImageView);
    if (!success) {
        return;
    }

    auto frameData = _dotSwapchain->get_current_frame_data();
    VkCommandBuffer cmd = frameData.mainCommandBuffer;
    _begin_command_buffer(cmd);
    { // clang-format off
        // Prepare image for rendering
        VkExtent2D swapchainExtent = _dotSwapchain->get_swapchain_extent();
        _drawExtent.height =
          std::min(swapchainExtent.height, _drawImage.imageExtent.height) * _renderScale;
        _drawExtent.width =
          std::min(swapchainExtent.width, _drawImage.imageExtent.width) * _renderScale;

        // Prepare for draw
        transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        draw_background(cmd);

        draw_geometry(cmd);

        // Prepare for copying
        transition_image( cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transition_image( cmd, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_image_to_image(cmd, _drawImage.image, swapchainImage, _drawExtent, _dotSwapchain->get_swapchain_extent());
        transition_image(cmd, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        draw_imgui(cmd, swapchainImageView);

        // Prepare for presentation
        transition_image(cmd, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } // clang-format on

    _end_command_buffer(cmd);
    _dotSwapchain->submit_command_buffer(cmd);

    // TODO: Move next frame into swapchain present image
    if (!_dotSwapchain->swapchain_present_image(_dotDevice->get_graphics_queue())) {
    }
    _frame++;
}

// NOTE: Maybe move to own class
void VkBackend::_end_command_buffer(VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));
}

void VkBackend::_begin_command_buffer(VkCommandBuffer cmd) {
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo =
      vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
}

void VkBackend::draw_background(VkCommandBuffer cmd) {
    // VkClearColorValue clearValue;
    // float flash = abs(sin(_dotSwapchain->get_current_frame() / 120.f));
    // clearValue = { { 0.0f, 0.0f, flash, 1.0f } };
    // VkImageSubresourceRange clearRange =
    // vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT); vkCmdClearColorImage(
    //   cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    const ComputeEffect& effect = _computeEffects[_currentBackgroundEffect];
    // _dotPipeline->get_background_effects()[_dotPipeline->get_current_background_effect()];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline->get_pipeline());
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            effect.pipeline->get_pipeline_layout(),
                            0,
                            1,
                            &_drawImageDescriptors,
                            0,
                            nullptr);

    vkCmdPushConstants(cmd,
                       effect.pipeline->get_pipeline_layout(),
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(PushConstants),
                       &effect.data);

    // DotPipeline::ComputePushConstants pc;
    // pc.data1 = glm::vec4(1, 0, 0, 1);
    // pc.data2 = glm::vec4(0, 0, 1, 1);
    // vkCmdPushConstants(cmd,
    //                    _dotPipeline->get_gradient_pipeline_layout(),
    //                    VK_SHADER_STAGE_COMPUTE_BIT,
    //                    0,
    //                    sizeof(DotPipeline::ComputePushConstants),
    //                    &pc);

    int32_t width_group_size = std::ceil(_drawImage.imageExtent.width / 16.0);
    uint32_t height_group_size = std::ceil(_drawImage.imageExtent.height / 16.0);
    vkCmdDispatch(cmd, width_group_size, height_group_size, 1);
}

void VkBackend::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment =
      vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo =
      vkinit::rendering_info(_dotSwapchain->get_swapchain_extent(), &colorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void VkBackend::draw_geometry(VkCommandBuffer cmd) {
    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

    for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
        if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj)) {
            opaque_draws.push_back(i);
        }
        opaque_draws.push_back(i);
    }

    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
        const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
        const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
        if (A.material == B.material) {
            return A.indexBuffer < B.indexBuffer;
        } else {
            return A.material < B.material;
        }
    });

    // begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment =
      vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      _depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo =
      vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    auto start = std::chrono::system_clock::now();
    stats.drawcallCount = 0;
    stats.triangleCount = 0;

    VkDevice device = _dotDevice->get_device();

    VkDescriptorSet globalDescriptor =
      _dotSwapchain->get_current_frame_data().frameDescriptors.allocate(
        device, _gpuSceneDataDescriptorLayout);
    /* Update global descriptor set */ {
        AllocatedBuffer gpuSceneDataBuffer = create_buffer(_allocator,
                                                           sizeof(GPUSceneData),
                                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
        DeletionQueue& queue = _dotSwapchain->get_current_frame_data().deletionQueue;
        queue.push_function([=, this]() { destroy_buffer(_allocator, gpuSceneDataBuffer); });

        // write the buffer
        GPUSceneData* sceneUniformData =
          (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
        *sceneUniformData = sceneData;

        DescriptorWriter writer;
        writer.write_buffer(
          0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.update_set(device, globalDescriptor);
    }

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    /* draw nodes */ {
        auto draw = [&](const RenderObject& ro) {
            if (ro.material != lastMaterial) {
                lastMaterial = ro.material;
                if (ro.material->pipeline != lastPipeline) {
                    lastPipeline = ro.material->pipeline;
                    vkCmdBindPipeline(
                      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ro.material->pipeline->pipeline);
                    vkCmdBindDescriptorSets(cmd,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            ro.material->pipeline->layout,
                                            0,
                                            1,
                                            &globalDescriptor,
                                            0,
                                            nullptr);

                    // set dynamic viewport and scissor
                    VkViewport viewport = {};
                    viewport.x = 0;
                    viewport.y = 0;
                    viewport.width = _drawExtent.width;
                    viewport.height = _drawExtent.height;
                    viewport.minDepth = 0.f;
                    viewport.maxDepth = 1.f;

                    vkCmdSetViewport(cmd, 0, 1, &viewport);

                    VkRect2D scissor = {};
                    scissor.offset.x = 0;
                    scissor.offset.y = 0;
                    scissor.extent.width = _drawExtent.width;
                    scissor.extent.height = _drawExtent.height;
                    vkCmdSetScissor(cmd, 0, 1, &scissor);
                    // launch a draw command to draw 3 vertices
                }
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        ro.material->pipeline->layout,
                                        1,
                                        1,
                                        &ro.material->materialSet,
                                        0,
                                        nullptr);
            }

            if (ro.indexBuffer != lastIndexBuffer) {
                lastIndexBuffer = ro.indexBuffer;
                vkCmdBindIndexBuffer(cmd, ro.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            }

            GPUDrawPushConstants pushConstants;
            pushConstants.vertexBuffer = ro.vertexBufferAddress;
            pushConstants.worldMatrix = ro.transform;
            vkCmdPushConstants(cmd,
                               ro.material->pipeline->layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(GPUDrawPushConstants),
                               &pushConstants);

            vkCmdDrawIndexed(cmd, ro.indexCount, 1, ro.firstIndex, 0, 0);
            stats.drawcallCount++;
            stats.triangleCount += ro.indexCount / 3;
        };
        for (int r : opaque_draws) {
            draw(mainDrawContext.OpaqueSurfaces[r]);
        }
        for (const RenderObject& r : mainDrawContext.TransparentSurfaces) {
            draw(r);
        }
        mainDrawContext.OpaqueSurfaces.clear();
        mainDrawContext.TransparentSurfaces.clear();
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.meshDrawTime = elapsed.count() / 1000.f;
    vkCmdEndRendering(cmd);
}

void VkBackend::resize_swapchain() {
    _dotSwapchain->resize_swapchain(_dotWindow.get_window_extent());
}

void VkBackend::wait_device_idle() {
    vkDeviceWaitIdle(_dotDevice->get_device());
}

// void DotRenderer::bind_descriptor_set(VkCommandBuffer cmd) {
//
// VkDescriptorSet descriptorSet = _dotPipeline->get_descriptor_set();
// }

// NOTE: Create a mesh class?
GPUMeshBuffers VkBackend::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer =
      create_buffer(_allocator,
                    vertexBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType =
                                                  VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress =
      vkGetBufferDeviceAddress(_dotDevice->get_device(), &deviceAdressInfo);

    // create index buffer
    newSurface.indexBuffer =
      create_buffer(_allocator,
                    indexBufferSize,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);
    // Second part.
    AllocatedBuffer staging = create_buffer(_allocator,
                                            vertexBufferSize + indexBufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    _dotSwapchain->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(_allocator, staging);

    return newSurface;
}

void VkBackend::write_texture_data(void* data, const AllocatedImage& image, bool mipmapped) {
    size_t data_size =
      image.imageExtent.depth * image.imageExtent.width * image.imageExtent.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(
      _allocator, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    _dotSwapchain->immediate_submit([&](VkCommandBuffer cmd) {
        transition_image(
          cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = image.imageExtent;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd,
                               uploadbuffer.buffer,
                               image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &copyRegion);

        if (mipmapped) {
            // generate_mipmaps(
              // cmd, image.image, VkExtent2D{ image.imageExtent.width, image.imageExtent.height });
        } else {
            transition_image(cmd,
                             image.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroy_buffer(_allocator, uploadbuffer);
}

void VkBackend::update_scene(glm::mat4 view, glm::mat4 projection) {
    sceneData.view = view;
    sceneData.proj = projection;
    sceneData.viewproj = projection * view;

    // some default lighting parameters
    sceneData.ambientColor = glm::vec4(.1f);
    sceneData.sunlightColor = glm::vec4(1.f);
    sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
    loadedScenes["structure"]->draw(glm::mat4{ 1.f }, mainDrawContext);
}

void GLTFMetallic_Roughness::build_pipelines(VkBackend* engine) {
    // VkDevice device = engine->_dotDevice->get_device();
    VkDevice device = engine->get_device().get_device();
    VkShaderModule meshFragShader;
    if (!vkinit::load_shader_module("build/built_shaders/mesh.frag.spv", device, &meshFragShader)) {
        fmt::println("Error when building the triangle fragment shader module");
    }

    VkShaderModule meshVertexShader;
    if (!vkinit::load_shader_module(
          "build/built_shaders/mesh.vert.spv", device, &meshVertexShader)) {
        fmt::println("Error when building the triangle vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout =
      layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = { engine->get_descriptor_layout(), materialLayout };

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    DotPipeline::PipelineBuilder pipelineBuilder;
    opaquePipeline.pipeline =
      pipelineBuilder.set_pipeline_layout(newLayout)
        .set_shaders(meshVertexShader, meshFragShader)
        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
        .set_multisampling_none()
        .disable_blending()
        .enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .set_color_attachment_format(engine->get_draw_image().imageFormat) // render format
        .set_depth_format(engine->get_depth_image().imageFormat)
        .build_pipeline(device);

    transparentPipeline.pipeline = pipelineBuilder.enable_blending_additive()
                                     .enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL)
                                     .build_pipeline(device);

    vkDestroyShaderModule(device, meshFragShader, nullptr);
    vkDestroyShaderModule(device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device) {
    // void DotRenderer::GLTFMetallic_Roughness::clear_resources()
    // std::cout << "Destroying pipeline" << std::endl;
    vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);
    // Shaderd layout so no need to destroy twice
    PRINT_D("Destroying opaque pipeline layout");
    PRINT_D("Destroying opaque pipeline");
    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    // vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);

    PRINT_D("Destroying transp pipeline layout");
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
    PRINT_D("Destroying transp pipeline");
    vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(
  VkDevice device,
  MaterialPass pass,
  const MaterialResources& resources,
  DescriptorAllocatorGrowable& descriptorAllocator) {
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
        matData.pipeline = &transparentPipeline;
    } else {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0,
                        resources.dataBuffer,
                        sizeof(MaterialConstants),
                        resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1,
                       resources.colorImage.imageView,
                       resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2,
                       resources.metalRoughImage.imageView,
                       resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx) {
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces) {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = &s.material->data;
        def.bounds = s.bounds;

        def.transform = nodeMatrix;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        ctx.OpaqueSurfaces.push_back(def);
    }

    Node::draw(topMatrix, ctx);
}

bool is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
    std::array<glm::vec3, 8> corners{
        glm::vec3{ 1, 1, 1 },   glm::vec3{ 1, 1, -1 },   glm::vec3{ 1, -1, 1 },
        glm::vec3{ 1, -1, -1 }, glm::vec3{ -1, 1, 1 },   glm::vec3{ -1, 1, -1 },
        glm::vec3{ -1, -1, 1 }, glm::vec3{ -1, -1, -1 },
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v =
          matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}

} // namespace dt
