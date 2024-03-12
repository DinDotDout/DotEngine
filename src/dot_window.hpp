#pragma once
#include "SDL_events.h"
#include "SDL_video.h"
#include <vulkan/vulkan_core.h>

namespace dt {
class DotWindow {
  public:
    DotWindow(uint32_t w, uint32_t h, std::string name);
    DotWindow(const DotWindow&) = delete;
    DotWindow& operator=(const DotWindow&) = delete;

    // SDL_Window* get_window() const { return _window; }
    VkExtent2D get_window_extent() const { return _windowExtent; }

    void update_window() {
        int w, h;
        SDL_GetWindowSize(_window, &w, &h);
        _windowExtent.width = w;
        _windowExtent.height = h;
    }

    void create_surface(VkInstance instance, VkSurfaceKHR* surface);

    void handle_window_events(const SDL_Event& event);
    bool should_stop_rendering() const { return _stop_rendering; }

    void request_resize() {
        update_window();
        _just_resized = true;
    }

    bool is_resize_requested() const { return _just_resized; }
    void reset_resize_request() { _just_resized = false; }

    ~DotWindow();

  private:
    bool _stop_rendering{ false };
    bool _just_resized{ false };

    const SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    std::string _name;
    VkExtent2D _windowExtent{ 1700, 900 };
    SDL_Window* _window;
};
}
