#include "SDL_video.h"
#include "dot_device.hpp"
// #include "dot_window.hpp"

// class DotWindow;
// class DotDevice;
namespace dt {
class DotImGui {
  public:
    // DotImGui(const DotDevice& dotDevice, const DotWindow& dotWindow);
    DotImGui(const DotDevice& dotDevice, SDL_Window* window, VkFormat imageFormat, VkInstance instance );
    DotImGui(DotImGui&&) = default;
    DotImGui(const DotImGui&) = default;
    DotImGui& operator=(DotImGui&&) = default;
    DotImGui& operator=(const DotImGui&) = default;
    ~DotImGui();

  private:
    VkDescriptorPool imguiPool;
    const DotDevice& _dotDevice;
    // const DotWindow& _dotWinow;
    void _init_imgui(const DotDevice& dotDevice, SDL_Window* window, VkFormat imageFormat, VkInstance instance);
};
}
