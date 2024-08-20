#pragma once
#include <filesystem>

#include "fastgltf/types.hpp"

#include "vk_renderer/core/dot_descriptors.hpp"
#include "vk_renderer/utils/dot_images.hpp"
#include <vk_renderer/vk_types.hpp>

namespace dt {
class VkBackend;

struct GLTFMaterial {
    MaterialInstance data;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(
  VkBackend* renderer,
  std::filesystem::path filePath);

struct LoadedGLTF : public IRenderable {
    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    VkBackend* creator;

    ~LoadedGLTF() { clear_all(); };

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx);

  private:
    void clear_all();
};

// std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VkBackend* engine, std::string_view
// filePath);
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VkBackend* engine,
                                                     std::filesystem::path filePath);
std::optional<AllocatedImage> load_image(VkBackend* engine,
                                         fastgltf::Asset& asset,
                                         fastgltf::Image& image);
} // namespace dt
