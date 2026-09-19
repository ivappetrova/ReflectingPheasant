#include "DebugDemoScene.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

DebugDemoScene::DebugDemoScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void DebugDemoScene::OnInit([[maybe_unused]] VulkanRenderer* renderer) {
    // Ground plane
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color(0.3f, 0.4f, 0.3f)));

    // Central sphere that will be animated

    m_sphereId = AddSphere(m_sphereStartPosition, 1.5f,
        Scene::Material::Metal(Scene::Color(0.8f, 0.3f, 0.3f), 0.2f));

    // Some static spheres for reference
    AddSphere(TriVector(5.0f, 1.0f, 0.0f), 1.0f,
        Scene::Material::Glossy(Scene::Color(0.3f, 0.3f, 0.8f), 0.1f));
    AddSphere(TriVector(-5.0f, 1.0f, 0.0f), 1.0f,
        Scene::Material::Glossy(Scene::Color(0.3f, 0.8f, 0.3f), 0.1f));
    AddSphere(TriVector(0.0f, 1.0f, 5.0f), 1.0f,
        Scene::Material::Glossy(Scene::Color(0.8f, 0.8f, 0.3f), 0.1f));

    // Point light
    AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                  Scene::Color(1.0f, 1.0f, 1.0f), 1.5f, 50.0f);

    // Camera
    m_cameraEye = TriVector(15.0f, 10.0f, 15.0f);
    m_cameraTarget = TriVector(0.0f, 3.0f, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
}

void DebugDemoScene::OnUpdate(float deltaTime) {
    UpdateFPS(deltaTime);

    if (!m_paused) {
        m_time += deltaTime * m_PHEASANT_ROTATION_SPEED;
    }

    // Animate the main sphere in a circle
    if (auto* sphere = GetSphere(m_sphereId))
    {
        const float radius = 3.0f;

        Motor motor{1/(deltaTime * m_PHEASANT_ROTATION_SPEED), std::cos(m_time) * radius, std::sin(m_time * 3), std::sin(m_time) * radius, 0, 0, 0, 0};
        sphere->SetCenter((motor * sphere->GetCenter() * ~motor).Grade3());
    }

    // === Debug Visualization ===

    // Draw world axes at origin
    if (m_showAxes) {
        DrawDebugAxes(TriVector(0.0f, 0.0f, 0.0f), 5.0f);

        // Draw axes at animated sphere position
        if (auto* sphere = GetSphere(m_sphereId)) {
            DrawDebugAxes(TriVector(sphere->center[0], sphere->center[1], sphere->center[2]), 1.0f);
        }
    }

    // Draw a grid on the ground plane
    if (m_showGrid) {
        const float gridSize = 10.0f;
        const float gridStep = 2.0f;
        const Scene::Color gridColor(0.5f, 0.5f, 0.5f);

        for (float x = -gridSize; x <= gridSize; x += gridStep) {
            DrawDebugLine(x, 0.01f, -gridSize, x, 0.01f, gridSize, gridColor);
        }
        for (float z = -gridSize; z <= gridSize; z += gridStep) {
            DrawDebugLine(-gridSize, 0.01f, z, gridSize, 0.01f, z, gridColor);
        }
    }

    // Draw points at each static sphere
    DrawDebugPoint(TriVector(5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Cyan());
    DrawDebugPoint(TriVector(-5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Magenta());
    DrawDebugPoint(TriVector(0.0f, 1.0f, 5.0f, 1.0f), 0.3f, Scene::Color::Yellow());

    // Draw BiVector lines (Plücker lines)
    if (m_showLines) {
        // Create lines using regressive product (join) of two points: p1 & p2
        TriVector p1(0.0f, 2.0f, 0.0f, 1.0f);  // Point at (0, 2, 0)
        TriVector p2(5.0f, 4.0f, 5.0f, 1.0f);  // Point at (5, 4, 5)
        BiVector line1 = p1 & p2;
        DrawDebugLine(line1, 8.0f, Scene::Color::Yellow());

        // Animated rotating line
        const float angle = m_time;
        TriVector pA(0.0f, 5.0f, 0.0f, 1.0f);
        TriVector pB(3.0f, 5.0f, 3.0f, 1.0f);
        BiVector line2 = pA & pB;

        Motor rotation{cos(angle), 0, 0, 0, 0, sin(angle) , 0, 0};
        BiVector rotatedLine = (rotation * line2 * ~rotation).Grade2();
        DrawDebugLine(rotatedLine, 6.0f, Scene::Color::Cyan());

        // Line connecting two static spheres
        TriVector sphereA(5.0f, 1.0f, 0.0f, 1.0f);
        TriVector sphereB(-5.0f, 1.0f, 0.0f, 1.0f);
        BiVector connectingLine = sphereA & sphereB;
        DrawDebugLine(connectingLine, 7.0f, Scene::Color::Magenta());
    }
}

void DebugDemoScene::OnInput(const InputState& input) {
    // Camera orbit with right mouse
    if (input.rightMouseDown) {
        m_cameraYaw += input.mouseDeltaX * m_mouseSensitivity;
        m_cameraPitch += input.mouseDeltaY * m_mouseSensitivity;
        m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);
    }

    // Zoom with scroll
    m_cameraDistance -= input.scrollDelta * 2.0f;
    m_cameraDistance = std::clamp(m_cameraDistance, 8.0f, 50.0f);

    // Update camera position
    constexpr float targetY = 3.0f;
    const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
    const float camY = std::sin(m_cameraPitch) * m_cameraDistance + targetY;
    const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

    m_cameraEye = TriVector(camX, camY, camZ);
    m_cameraTarget = TriVector(0.0f, targetY, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);

    // Toggle keys (only on key press, not while held)
    if (input.keyP && !m_prevKeyP) m_paused = !m_paused;
    if (input.key1 && !m_prevKey1) m_showAxes = !m_showAxes;
    if (input.key2 && !m_prevKey2) m_showGrid = !m_showGrid;
    if (input.key3 && !m_prevKey3) m_showLines = !m_showLines;
    if (input.keyR && !m_prevKeyR) Reset();

    // Store key states for next frame
    m_prevKeyP = input.keyP;
    m_prevKey1 = input.key1;
    m_prevKey2 = input.key2;
    m_prevKey3 = input.key3;
    m_prevKeyR = input.keyR;
}

void DebugDemoScene::OnGui() {
    ImGui::Begin("Debug Visualization Demo");
    ImGui::Text("FPS: %.1f", GetFPS());

    ImGui::Separator();
    ImGui::Text("Animation");
    ImGui::Checkbox("Paused (P)", &m_paused);
    ImGui::SliderFloat("Speed", &m_PHEASANT_ROTATION_SPEED, 0.0f, 3.0f);
    if (ImGui::Button("Reset (R)")) {
        this->Reset();
    }

    ImGui::Separator();
    ImGui::Text("Debug Visualization");
    ImGui::Checkbox("Show Axes (1)", &m_showAxes);
    ImGui::Checkbox("Show Grid (2)", &m_showGrid);
    ImGui::Checkbox("Show BiVector Lines (3)", &m_showLines);

    ImGui::Separator();
    ImGui::Text("Controls");
    ImGui::Text("  Right-drag: Orbit camera");
    ImGui::Text("  Scroll: Zoom");
    ImGui::Text("  P: Pause animation");
    ImGui::Text("  1-3: Toggle visualizations");
    ImGui::Text("  R: Reset time");

    ImGui::End();

    // Render debug lines
    RenderDebugDraw();
}

void DebugDemoScene::Reset()
{
    m_time = 0.0f;
    GetSphere(m_sphereId)->SetCenter(m_sphereStartPosition);
}

void DebugDemoScene::OnShutdown() {}
