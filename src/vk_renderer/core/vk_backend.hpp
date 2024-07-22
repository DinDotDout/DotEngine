#pragma once
#include "VkBootstrap.h"
#include "memory"

#include "vk_renderer/utils/deletion_queue.hpp"
#include "vk_renderer/utils/dot_loader.hpp"
#include <vk_renderer/core/dot_descriptors.hpp>
#include <vk_renderer/utils/dot_images.hpp>
#include <vk_renderer/vk_types.hpp>
// #include "vk_renderer/utils/dot_images.hpp"

namespace dt {

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> OpaqueSurfaces;
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct GLTFMetallic_Roughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // padding, we need it anyway for uniform buffers
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void build_pipelines(VkBackend* engine);
    void clear_resources(VkDevice device);
    MaterialInstance write_material(VkDevice device,
                                    MaterialPass pass,
                                    const MaterialResources& resources,
                                    DescriptorAllocatorGrowable& descriptorAllocator);
};

class DotDevice;
class DotSwapchain;
class DotWindow;
class DotPipeline;
class DotImGui;

class VkBackend {
  public:
    // NOTE: Should layout be included?
    struct GPUSceneData {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewproj;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection; // w for sun power
        glm::vec4 sunlightColor;
    };

    struct PushConstants {
        glm::vec4 data1;
        glm::vec4 data2;
        glm::vec4 data3;
        glm::vec4 data4;
        // float data[64]; // Maximum size for push constants is 128 bytes
    };

    struct ComputeEffect {
        const char* name;
        std::unique_ptr<DotPipeline> pipeline;
        PushConstants data;
        // float data[64]; // Maximum size for push constants is 128 bytes
    };

    explicit VkBackend(DotWindow& dotWindow);
    ~VkBackend();

    VkBackend(const VkBackend&) = delete;
    VkBackend& operator=(const VkBackend&) = delete;

    void render();
    void update_scene(glm::mat4 view, glm::mat4 projection);
    void resize_swapchain();
    void wait_device_idle();
    const std::vector<ComputeEffect>& get_background_effects() const { return _computeEffects; };

    VkDescriptorSetLayout& get_descriptor_layout() { return _gpuSceneDataDescriptorLayout; };
    int get_current_background_effect() const { return _currentBackgroundEffect; };
    float get_render_scale() const { return _renderScale; };

    DotDevice& get_device() { return *_dotDevice; };
    AllocatedImage& get_draw_image() { return _drawImage; };
    AllocatedImage& get_depth_image() { return _depthImage; };

    void set_current_background_effect(int backgroundEffect) {
        _currentBackgroundEffect = backgroundEffect;
    }
    void set_render_scale(float renderScale) { _renderScale = renderScale; }

    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

  private:
    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;

    uint32_t _frame = 0;
    DotWindow& _dotWindow;

#ifdef NDEBUG
    const bool _useValidationLayers = false;
#else
    const bool _useValidationLayers = true;
#endif

    VkDebugUtilsMessengerEXT _debugMessenger;
    VkSurfaceKHR _surface{};
    VkInstance _instance{};

    std::unique_ptr<DotDevice> _dotDevice;
    std::unique_ptr<DotSwapchain> _dotSwapchain;
    std::unique_ptr<DotImGui> _dotImGui;

    DescriptorAllocatorGrowable globalDescriptorAllocator;
    VkDescriptorSet
      _drawImageDescriptors; // Descriptor set for the image this is going to work onto
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // NOTE: Should not be embedded. Maybe keep some default pipeline around.
    int _currentBackgroundEffect{ 0 };
    std::vector<ComputeEffect> _computeEffects;
    std::unique_ptr<DotPipeline> _trianglePipeline;

    std::unique_ptr<DotPipeline>
      _meshPipeline; // NOTE: Bind pipeline with multiple meshes or some other thing.
    GPUMeshBuffers rectangle;
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    GPUSceneData sceneData;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
    VkDescriptorSetLayout _singleImageDescriptorLayout;

    DeletionQueue _deletionQueue{};
    VmaAllocator _allocator;
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;
    float _renderScale = 1.0f;


    // <engine defaults
    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;
    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;
    // </engine defaults

    void _init_vulkan();
    vkb::Instance _create_instance();
    bool _create_surface();
    void _init_device(vkb::Instance& vkb_inst);
    void _init_swapchain();
    void _init_allocator();
    void _create_draw_image_and_view();
    void _init_descriptors();
    void _init_pipelines();
    void _init_imgui();
    void _init_default_data();

    // void draw(VkCommandBuffer cmd, VkImage* im);
    void draw_background(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void draw_geometry(VkCommandBuffer cmd);

    // void bind_descriptor_set(VkCommandBuffer cmd);
    void _end_command_buffer(VkCommandBuffer cmd);
    void _begin_command_buffer(VkCommandBuffer cmd);

    void write_texture_data(void* data, const AllocatedImage& image);
    // AllocatedImage create_image(VmaAllocator allocator,
    //                             void* data,
    //                             VkExtent3D size,
    //                             VkFormat format,
    //                             VkDevice device,
    //                             VkImageUsageFlags usage,
    //                             bool mipmapped = false);
};

} // namespace dt
