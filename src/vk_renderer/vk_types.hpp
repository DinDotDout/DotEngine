// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <span>

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace dt{
struct DrawContext;
class IRenderable {
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable {
    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix) {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) {
        // draw children
        for (auto& c : children) {
            c->draw(topMatrix, ctx);
        }
    }
};

enum class MaterialPass : uint8_t { MainColor, Transparent, Other };
struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};
struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


// TODO: Change to struct of arrays.
struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// NOTE: Global deafult for our shaders. (First uniform buffer)
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

#ifdef NDEBUG
#define VK_CHECK(x) x
#define PRINT_D(message, ...)
#else
#define VK_CHECK(x)                                                                                \
    do {                                                                                           \
        VkResult err = x;                                                                          \
        if (err < 0) {                                                                             \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err));                       \
            abort();                                                                               \
        }                                                                                          \
    } while (0)
#define PRINT_D(message, ...) fmt::println(message, ##__VA_ARGS__);
#endif
}
