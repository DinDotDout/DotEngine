#include "SDL_video.h"
// #include "dot_device.hpp"
namespace dt {
class DotDevice;
class DotImGui {
  public:
    DotImGui(const DotDevice& dotDevice, SDL_Window* window, VkFormat imageFormat, VkInstance instance );
    DotImGui(DotImGui&&) = default;
    DotImGui(const DotImGui&) = default;
    DotImGui& operator=(DotImGui&&) = delete;
    DotImGui& operator=(const DotImGui&) = delete;
    ~DotImGui();

  private:
    VkDescriptorPool imguiPool;
    const DotDevice& _dotDevice;
    void _init_imgui(const DotDevice& dotDevice, SDL_Window* window, VkFormat imageFormat, VkInstance instance);
};
}
