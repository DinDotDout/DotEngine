#pragma once

#include "dot_device.hpp"
#include "dot_swapchain.hpp"
#include "dot_window.hpp"

namespace dt {
class DotRenderer {

  public:
    DotRenderer(DotWindow& dotWindow, DotDevice& dotDevice);
    ~DotRenderer() = default;

    DotRenderer(const DotRenderer&) = delete;
    DotRenderer& operator=(const DotRenderer&) = delete;

    void render_pass();
    void resize_swapchain();

    // Restructure
    // void next_frame() { _dotSwapchain.next_frame(); }
    // int get_frame_number() const { return _dotSwapchain.get_current_frame(); }
    // VkImage get_image(uint32_t i) const { return _dotSwapchain.get_image(i); }
    // DotSwapchain::FrameData& get_current_frame_data() {
    //     return _dotSwapchain.get_current_frame_data();
    // };
    VkSwapchainKHR get_swapchain() const { return _dotSwapchain.get_swapchain(); }

  private:
    DotWindow& _dotWindow;
    DotDevice& _dotDevice;
    DotSwapchain _dotSwapchain;

    void draw(VkCommandBuffer cmd, VkImage currentImage);
    void record_command_buffer(VkCommandBuffer* cmd, VkImage* swapchainImage);
};
}
