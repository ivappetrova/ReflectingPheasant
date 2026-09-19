# Debug UI (ImGui)

FlyTracer integrates [Dear ImGui](https://github.com/ocornut/imgui) for debug interfaces. Use it to display stats, tweak parameters, and debug your scene.

## Basic Usage

Override `OnGui` to create your debug UI:

```cpp
#include <imgui.h>

void MyScene::OnGui() {
    ImGui::Begin("My Debug Window");
    ImGui::Text("Hello, FlyTracer!");
    ImGui::End();
}
```

## Common Widgets

### Text Display

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Stats");

    // Simple text
    ImGui::Text("Frame time: %.3f ms", deltaTime * 1000.0f);

    // Colored text
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Warning: Low FPS!");

    // Formatted text
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)",
                m_cameraEye.e032(), m_cameraEye.e013(), m_cameraEye.e021());

    // Built-in FPS helper
    ImGui::Text("FPS: %.1f", GetFPS());

    ImGui::End();
}
```

### Sliders and Inputs

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Parameters");

    // Float slider
    ImGui::SliderFloat("Speed", &m_speed, 0.0f, 10.0f);

    // Float slider with format
    ImGui::SliderFloat("Angle", &m_angle, 0.0f, 360.0f, "%.1f deg");

    // Integer slider
    ImGui::SliderInt("Count", &m_count, 1, 100);

    // Drag (no min/max)
    ImGui::DragFloat("Position X", &m_posX, 0.1f);

    // Input field
    ImGui::InputFloat("Precise Value", &m_value, 0.01f, 0.1f);

    // Color picker
    ImGui::ColorEdit3("Light Color", m_lightColor);

    ImGui::End();
}
```

### Buttons and Checkboxes

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Controls");

    // Button
    if (ImGui::Button("Reset Scene")) {
        ResetScene();
    }

    // Same line buttons
    ImGui::SameLine();
    if (ImGui::Button("Randomize")) {
        Randomize();
    }

    // Checkbox
    ImGui::Checkbox("Show Grid", &m_showGrid);
    ImGui::Checkbox("Animate", &m_animate);

    // Radio buttons
    ImGui::RadioButton("Mode A", &m_mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Mode B", &m_mode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Mode C", &m_mode, 2);

    ImGui::End();
}
```

### Combo Box (Dropdown)

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Options");

    const char* items[] = { "Flat", "Lambert", "Phong", "PBR" };
    ImGui::Combo("Shading", &m_selectedShading, items, IM_ARRAYSIZE(items));

    ImGui::End();
}
```

### Separators and Headers

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Organized Window");

    // Collapsible header
    if (ImGui::CollapsingHeader("Camera Settings")) {
        ImGui::SliderFloat("FOV", &m_cameraFov, 20.0f, 120.0f);
        ImGui::DragFloat3("Position", m_cameraPos);
    }

    ImGui::Separator();  // Horizontal line

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Ambient", m_ambientColor);
        ImGui::SliderFloat("Intensity", &m_lightIntensity, 0.0f, 5.0f);
    }

    ImGui::End();
}
```

## ImGui Resources

- [ImGui Demo](https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp) - Comprehensive examples of all widgets
- [ImGui Manual](https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html) - Interactive documentation
