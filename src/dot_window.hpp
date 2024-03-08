#pragma once
#include <vulkan/vulkan_core.h>
class SDL_Window;

namespace dt {
class DotWindow {
  public:
    DotWindow(uint32_t w, uint32_t h, std::string name);
    DotWindow(DotWindow&&) = default;
    DotWindow(const DotWindow&) = default;

    DotWindow& operator=(DotWindow&&) = delete;
    DotWindow& operator=(const DotWindow&) = delete;

    // SDL_Window* get_window() const { return _window; }
    VkExtent2D get_window_extent() const { return _windowExtent; }

    void create_surface(VkInstance instance, VkSurfaceKHR* surface);

    ~DotWindow();

  private:
    std::string _name;
    VkExtent2D _windowExtent{ 1700, 900 };
    SDL_Window* _window;
};
}
