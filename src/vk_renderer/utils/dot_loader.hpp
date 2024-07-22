#pragma once
// #include "vk_renderer/core/dot_descriptors.hpp"
// #include "vk_renderer/utils/dot_images.hpp"
#include <vk_renderer/vk_types.hpp>
// #include <unordered_map>
#include <filesystem>

namespace dt {
class VkBackend;

struct GLTFMaterial {
    MaterialInstance data;
};
struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
  VkBackend* renderer,
  std::filesystem::path filePath);

// struct LoadedGLTF : public IRenderable {
//     // storage for all the data on a given glTF file
//     std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
//     std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
//     std::unordered_map<std::string, AllocatedImage> images;
//     std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
//
//     // nodes that dont have a parent, for iterating through the file in tree order
//     std::vector<std::shared_ptr<Node>> topNodes;
//
//     std::vector<VkSampler> samplers;
//
//     DescriptorAllocatorGrowable descriptorPool;
//
//     AllocatedBuffer materialDataBuffer;
//
//     VkBackend* creator;
//
//     ~LoadedGLTF() { clearAll(); };
//
//     virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);
//
//   private:
//     void clearAll();
// };

} // namespace dt
