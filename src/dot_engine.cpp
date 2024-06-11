#include "dot_engine.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <iostream>
#include <thread>

namespace dt {
void DotEngine::run() {
    SDL_Event e;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                _dotRenderer.wait_device_idle();
                return;
            }
            if (e.type == SDL_WINDOWEVENT) {
                _dotWindow.handle_window_events(e);
            }
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (_dotWindow.is_minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_dotWindow.is_resize_requested()) {
            _dotRenderer.resize_swapchain();
            _dotWindow.reset_resized();
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();
        if (ImGui::Begin("background")) {

            int currentBackgroundEffect = _dotRenderer.get_current_background_effect();
            const std::vector<DotPipeline::ComputeEffect>& effects =
              _dotRenderer.get_background_effects();
            const DotPipeline::ComputeEffect& selected = effects[currentBackgroundEffect];
            ImGui::Text("Selected effect: ", selected.name);
            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, effects.size() - 1);
            _dotRenderer.set_current_background_effect(currentBackgroundEffect);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();
        ImGui::Render();

        // make imgui calcu
        _dotRenderer.render_pass();
    }
}
} // namespace dt
