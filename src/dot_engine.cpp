#include "dot_engine.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/trigonometric.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <SDL.h>
#include <thread>

namespace dt {

// Keep?
DotEngine* loadedEngine = nullptr;
DotEngine& DotEngine::get() {
    return *loadedEngine;
}


DotEngine::DotEngine() {
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    mainCamera.velocity = glm::vec3(0.f);
    // mainCamera.position = glm::vec3(0, 0, 5);
    mainCamera.position = glm::vec3(30.f, -00.f, -085.f);
    mainCamera.pitch = 0;
    mainCamera.yaw = 0;
}


void DotEngine::_draw_imgui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Stats")) {
        ImGui::Text("frametime %f ms", stats.frameTime);
        ImGui::Text("drawtime %f ms", _vkBackend.stats.meshDrawTime);
        ImGui::Text("triangles %i", _vkBackend.stats.triangleCount);
        ImGui::Text("draws %i", _vkBackend.stats.drawcallCount);
    }
    ImGui::End();

    if (ImGui::Begin("background")) {
        float renderScale = _vkBackend.get_render_scale();
        ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
        _vkBackend.set_render_scale(renderScale);
        int currentBackgroundEffect = _vkBackend.get_current_background_effect();
        const std::vector<VkBackend::ComputeEffect>& effects = _vkBackend.get_background_effects();
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
}

void DotEngine::_update_scene() {
    glm::mat4 view = mainCamera.getViewMatrix();
    VkExtent2D extent = _dotWindow.get_window_extent();
    glm::mat4 projection = glm::perspective(
      glm::radians(70.f), (float)extent.width / (float)extent.height, 10000.f, 0.1f);
    projection[1][1] *= -1;
    _vkBackend.update_scene(view, projection);
    _vkBackend.render();
}

void DotEngine::run() {
    SDL_Event e;
    SDL_SetRelativeMouseMode(SDL_TRUE);
    while (true) {
        auto start = std::chrono::high_resolution_clock::now();
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

        // SDL_SetRelativeMouseMode(_dotWindow.has_focus() ? SDL_TRUE : SDL_FALSE);
        if (_dotWindow.is_minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_dotWindow.is_resize_requested()) {
            _vkBackend.resize_swapchain();
            _dotWindow.reset_resized();
        }

        _draw_imgui();
        mainCamera.update();
        _update_scene();
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frameTime = elapsed.count() / 1000.f;
    }
    // SDL_SetRelativeMouseMode(SDL_FALSE);
}
} // namespace dt
