#include "dot_engine.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <thread>

namespace dt {

DotEngine::DotEngine() {
    mainCamera.velocity = glm::vec3(0.f);
    mainCamera.position = glm::vec3(0, 0, 5);

    mainCamera.pitch = 0;
    mainCamera.yaw = 0;
}

void DotEngine::run() {
    SDL_Event e;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                _vkBackend.wait_device_idle(); // Wait idle before cleanup
                return;
            }
            if (e.type == SDL_WINDOWEVENT) {
                _dotWindow.handle_window_events(e);
            }
            mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (_dotWindow.is_minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_dotWindow.is_resize_requested()) {
            _vkBackend.resize_swapchain();
            _dotWindow.reset_resized();
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();
        if (ImGui::Begin("background")) {
            float renderScale = _vkBackend.get_render_scale();
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
            _vkBackend.set_render_scale(renderScale);
            int currentBackgroundEffect = _vkBackend.get_current_background_effect();
            const std::vector<VkBackend::ComputeEffect>& effects =
              _vkBackend.get_background_effects();
            const VkBackend::ComputeEffect& selected = effects[currentBackgroundEffect];
            ImGui::Text("Selected effect: %s", selected.name);
            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, effects.size() - 1);
            _vkBackend.set_current_background_effect(currentBackgroundEffect);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();
        ImGui::Render();
        mainCamera.update();

        glm::mat4 view = mainCamera.getViewMatrix();
        VkExtent2D extent = _dotWindow.get_window_extent();
        glm::mat4 projection = glm::perspective(
          glm::radians(70.f), (float)extent.width / (float)extent.height, 10000.f, 0.1f);

        // invert the Y direction on projection matrix so that we are more similar
        // to opengl and gltf axis
        projection[1][1] *= -1;
        _vkBackend.update_scene(view, projection);
        _vkBackend.render();
    }
}
} // namespace dt
