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

    SDL_Window* get_window() { return _window; }
    const VkExtent2D& get_window_extent() const { return _windowExtent; }
    bool is_minimized() const { return _minimized; }
    bool is_resize_requested() const { return _window_resized; }
    void reset_resized() { _window_resized = false; }

    void update_extent();
    void handle_window_events(const SDL_Event& event);
    ~DotWindow();

  private:
    bool _minimized{ false };
    bool _window_resized{ false };

    const SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    std::string _name;
    VkExtent2D _windowExtent{ 1700, 900 };
    SDL_Window* _window;
};
} // namespace dt
