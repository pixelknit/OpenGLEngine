#ifndef UI_H
#define UI_H

#include "camera.h"
#include "scene_manager.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

// Every scene / render setting the editor panels can drive. main.cpp owns one
// of these, hands it to EditorUI::Draw() once per frame, then reads the values
// back when it fills in uniforms — the UI itself never touches GL state.
struct EditorState {
    // --- Sun (directional light) -------------------------------------------
    // Azimuth and elevation in degrees define the direction; distance only
    // pushes the shadow camera back along that direction, it doesn't affect
    // shading (the sun is directional).
    float sunAzimuth = -135.0f;
    float sunElevation = 64.8f;
    float sunDistance = 33.2f;
    float sunIntensity = 5.0f;
    glm::vec3 sunTint{1.0f, 1.0f, 1.0f};
    bool sunAnimate = false;
    float sunAnimateSpeed = 10.0f; // degrees of azimuth per second

    // Half-width of the light's orthographic shadow box, in world units.
    // Shrink it for crisper shadows, grow it to stop far objects clipping out.
    float shadowExtent = 35.0f;

    // --- Environment --------------------------------------------------------
    std::vector<std::string> hdriPaths; // populated by EditorUI::ScanHDRIs
    int hdriIndex = 0;
    float envMapIntensity = 1.0f;

    // --- PBR tweaks (mirror the uniforms of the same name in pbr.fs) --------
    float minRoughness = 0.35f;
    float metallicMult = 0.5f;

    // Elevation is clamped short of the poles: at exactly ±90° the shadow
    // camera's lookAt direction is parallel to its up vector and degenerates.
    static constexpr float kMaxElevation = 89.0f;

    glm::vec3 SunDirection() const {
        float az = glm::radians(sunAzimuth);
        float el = glm::radians(glm::clamp(sunElevation, -kMaxElevation, kMaxElevation));
        return glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
    }

    // Where to put the shadow camera so it looks at `target` from the sun's
    // direction. Shading itself only uses SunDirection().
    glm::vec3 ShadowCameraPos(const glm::vec3 &target) const {
        return target + SunDirection() * sunDistance;
    }

    glm::vec3 SunRadiance() const { return sunTint * sunIntensity; }
};

namespace EditorUI {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Call once after the GL context exists and after the app has installed its own
// GLFW callbacks — the ImGui backend chains onto them rather than replacing
// them, so ordering matters.
inline void Init(GLFWwindow *window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks*/ true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

inline void Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

inline void BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

// Draws the accumulated UI into the currently bound framebuffer. Must be the
// last thing before glfwSwapBuffers so the panels sit on top of the composite.
inline void EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// True while a panel is using the mouse / keyboard, so the camera and the
// game-side key handling should stand down.
inline bool WantsMouse() { return ImGui::GetIO().WantCaptureMouse; }
inline bool WantsKeyboard() { return ImGui::GetIO().WantCaptureKeyboard; }

// ---------------------------------------------------------------------------
// Asset discovery
// ---------------------------------------------------------------------------

// Finds every .hdr under `root` (recursively) so the environment dropdown can
// list them. Paths come back with forward slashes, matching the rest of the
// engine's asset paths.
inline std::vector<std::string> ScanHDRIs(const std::string &root) {
    namespace fs = std::filesystem;
    std::vector<std::string> found;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;

        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".hdr")
            found.push_back(it->path().generic_string());
    }

    std::sort(found.begin(), found.end());
    return found;
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------

namespace detail {

// Kept out of DrawSunSection so the sun keeps moving while that header is
// collapsed.
inline void UpdateSunAnimation(EditorState &s, float deltaTime) {
    if (!s.sunAnimate) return;
    s.sunAzimuth += s.sunAnimateSpeed * deltaTime;
    if (s.sunAzimuth > 180.0f) s.sunAzimuth -= 360.0f;
    if (s.sunAzimuth < -180.0f) s.sunAzimuth += 360.0f;
}

inline void DrawSunSection(EditorState &s) {
    if (!ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::SliderFloat("Azimuth", &s.sunAzimuth, -180.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Elevation", &s.sunElevation, -EditorState::kMaxElevation,
                       EditorState::kMaxElevation, "%.1f deg");
    ImGui::SliderFloat("Intensity", &s.sunIntensity, 0.0f, 30.0f, "%.2f");
    ImGui::ColorEdit3("Tint", glm::value_ptr(s.sunTint));

    ImGui::Checkbox("Animate", &s.sunAnimate);
    if (s.sunAnimate) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("deg/s", &s.sunAnimateSpeed, -90.0f, 90.0f, "%.0f");
    }

    ImGui::SeparatorText("Shadows");
    ImGui::SliderFloat("Distance", &s.sunDistance, 5.0f, 120.0f, "%.1f");
    ImGui::SetItemTooltip("How far back the shadow camera sits. Shading is unaffected.");
    ImGui::SliderFloat("Extent", &s.shadowExtent, 5.0f, 100.0f, "%.1f");
    ImGui::SetItemTooltip("Half-width of the shadow box. Smaller = sharper, but clips distant objects.");

    glm::vec3 dir = s.SunDirection();
    ImGui::TextDisabled("dir  %.2f, %.2f, %.2f", dir.x, dir.y, dir.z);
}

inline void DrawEnvironmentSection(EditorState &s) {
    if (!ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (s.hdriPaths.empty()) {
        ImGui::TextDisabled("No .hdr files found under models/");
    } else {
        // The combo shows bare filenames; hdriIndex indexes the full paths.
        std::string current = std::filesystem::path(s.hdriPaths[s.hdriIndex]).filename().string();
        if (ImGui::BeginCombo("HDRI", current.c_str())) {
            for (int i = 0; i < (int)s.hdriPaths.size(); ++i) {
                std::string label = std::filesystem::path(s.hdriPaths[i]).filename().string();
                if (ImGui::Selectable(label.c_str(), i == s.hdriIndex))
                    s.hdriIndex = i;
                if (i == s.hdriIndex) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("%s", s.hdriPaths[s.hdriIndex].c_str());
    }

    ImGui::SliderFloat("Env intensity", &s.envMapIntensity, 0.0f, 5.0f, "%.2f");
}

inline void DrawRenderingSection(EditorState &s) {
    if (!ImGui::CollapsingHeader("Rendering")) return;

    ImGui::SliderFloat("Min roughness", &s.minRoughness, 0.0f, 1.0f, "%.2f");
    ImGui::SetItemTooltip("Floor on roughness; keeps surfaces from going mirror-smooth.");
    ImGui::SliderFloat("Metallic mult", &s.metallicMult, 0.0f, 2.0f, "%.2f");
}

// Position / rotation / scale for a plain, non-instanced entity. These feed
// Transform::GetMatrix() every frame, so edits show up immediately.
inline void DrawTransformControls(Entity &entity) {
    ImGui::DragFloat3("Position", glm::value_ptr(entity.transform.position), 0.05f);
    ImGui::DragFloat3("Rotation", glm::value_ptr(entity.transform.rotation), 0.5f, -360.0f, 360.0f);
    ImGui::DragFloat3("Scale", glm::value_ptr(entity.transform.scale), 0.01f, 0.001f, 100.0f);
}

// Scatter parameters for an instanced cluster. Any change rebuilds the instance
// transforms and re-uploads them, which is a handful of buffer uploads — cheap
// enough to do live while a slider is being dragged.
inline void DrawScatterControls(Scene &scene, Entity &entity) {
    ScatterParams &p = entity.scatter;
    bool dirty = false;

    int count = (int)p.count;
    if (ImGui::SliderInt("Count", &count, 1, 500)) {
        p.count = (unsigned int)count;
        dirty = true;
    }

    dirty |= ImGui::DragFloat3("Center", glm::value_ptr(p.center), 0.05f);
    dirty |= ImGui::SliderFloat("Radius", &p.radius, 0.5f, 60.0f, "%.2f");
    dirty |= ImGui::DragFloatRange2("Scale range", &p.scaleRange.x, &p.scaleRange.y,
                                    0.05f, 0.01f, 50.0f, "min %.2f", "max %.2f");
    dirty |= ImGui::SliderFloat("Yaw jitter", &p.yRotationJitter, 0.0f, 360.0f, "%.0f deg");

    int seed = (int)p.seed;
    if (ImGui::SliderInt("Seed", &seed, 0, 10000)) {
        p.seed = (unsigned int)seed;
        dirty = true;
    }
    ImGui::SetItemTooltip("Reshuffles every instance's position, yaw and scale.");

    ImGui::SameLine();
    if (ImGui::Button("Randomize")) {
        static std::mt19937 rng(std::random_device{}());
        p.seed = std::uniform_int_distribution<unsigned int>(0, 10000)(rng);
        dirty = true;
    }

    if (dirty) scene.Rescatter(entity);
}

inline void DrawSceneSection(Scene &scene) {
    if (!ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) return;

    for (size_t i = 0; i < scene.entities.size(); ++i) {
        Entity &entity = scene.entities[i];
        // Names aren't guaranteed unique, so scope widget IDs by index.
        ImGui::PushID((int)i);

        if (ImGui::TreeNode(entity.name.empty() ? "entity" : entity.name.c_str())) {
            if (entity.scatter.enabled) {
                DrawScatterControls(scene, entity);
            } else {
                DrawTransformControls(entity);
            }

            if (entity.lodModels.size() > 1) {
                // UpdateLOD() swapped in one of the LOD meshes for this frame;
                // report which, so the distance thresholds can be sanity-checked.
                size_t active = 0;
                for (size_t l = 0; l < entity.lodModels.size(); ++l)
                    if (entity.lodModels[l] == entity.model) active = l;
                ImGui::TextDisabled("LOD %d / %d", (int)active, (int)entity.lodModels.size() - 1);
            }
            if (entity.IsInstanced())
                ImGui::TextDisabled("%d instances", (int)entity.instanceTransforms.size());

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

} // namespace detail

// Builds every panel for this frame. `uiMode` is the app's cursor-released
// state, shown as a hint since the panels are visible but inert while the
// camera has the mouse captured.
inline void Draw(EditorState &state, Scene &scene, const Camera &camera,
                 float deltaTime, bool uiMode) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 640), ImGuiCond_FirstUseEver);
    ImGui::Begin("Retinal Engine");

    ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::TextDisabled("cam  %.1f, %.1f, %.1f", camera.Position.x, camera.Position.y, camera.Position.z);
    if (uiMode)
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "TAB: back to camera");
    else
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "TAB: release cursor to edit");
    ImGui::Separator();

    detail::UpdateSunAnimation(state, deltaTime);
    detail::DrawSunSection(state);
    detail::DrawEnvironmentSection(state);
    detail::DrawRenderingSection(state);
    detail::DrawSceneSection(scene);

    ImGui::End();
}

} // namespace EditorUI

#endif
