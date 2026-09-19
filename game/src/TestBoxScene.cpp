#include "TestBoxScene.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

TestBoxScene::TestBoxScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void TestBoxScene::OnInit([[maybe_unused]] VulkanRenderer* renderer) {
    constexpr float halfWidth = 50.0f;
    constexpr float height = 30.0f;

    // Bottom plane
    AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));

    // Top plane
    AddPlane(Vector(height, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.9f)));

    // Left plane
    AddPlane(Vector(halfWidth, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.5f, 0.5f)));

    // Right plane
    AddPlane(Vector(halfWidth, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.5f, 0.9f, 0.5f)));

    // Spheres
    m_miniSphereId = AddSphere(TriVector(m_MINI_SPHERE_RADIUS, m_sphere1Height, 0.0f), 5.0f,
              Scene::Material::Metal(Scene::Color(0.9f, 0.3f, 0.3f), 0.2f));

    m_sphere2Id = AddSphere(TriVector(-m_sphere2Radius, m_sphere2Height, 0.0f), 8.0f,
              Scene::Material::Glossy(Scene::Color(0.3f, 0.3f, 0.9f), 0.1f));

    // Pheasant mesh
    m_pheasantMeshId = LoadMesh("pheasant.obj", "pheasant.png");
    AddMeshInstance(m_pheasantMeshId, TriVector(0.0f, m_pheasantHeight, 0.0f), "pheasant");
    AddMeshInstance(m_pheasantMeshId, TriVector(5.0f, m_pheasantHeight, 0.0f), "pheasant2");


    if (auto* pheasant = FindInstance("pheasant")) {
        pheasant->scale = m_PHEASANT_SCALE;
    }

    // Point light
    AddPointLight(TriVector(0.0f, 20.0f, 0.0f),
                  Scene::Color(1.0f, 1.0f, 1.0f), 2.0f, 100.0f);

    // Camera
    m_cameraEye = TriVector(0.0f, 15.0f, 60.0f);
    m_cameraTarget = TriVector(0.0f, 10.0f, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f, 0.0f);
}

void TestBoxScene::OnUpdate(float deltaTime) {
    UpdateFPS(deltaTime);

    // Rotate spheres
    const float incrementAngle = m_PHEASANT_ROTATION_SPEED * deltaTime;
    const float incrementDegrees = incrementAngle * (180.0f / kPi);
    const BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    const Motor R = Motor::Rotation(incrementDegrees, yAxis);

    if (auto* sphere1 = GetSphere(m_miniSphereId)) {
        sphere1->SetCenter((R * sphere1->GetCenter() * ~R).Grade3());
    }
    if (auto* sphere2 = GetSphere(m_sphere2Id)) {
        sphere2->SetCenter((R * sphere2->GetCenter() * ~R).Grade3());
    }

    // Update pheasant
    m_pheasantTime += m_pheasantSpeed * deltaTime;

    if (auto* pheasant = FindInstance("pheasant")) {
        const float pheasantX = std::sin(m_pheasantTime) / 10.0f;
        const Motor translation(1.0f, pheasantX * 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        pheasant->transform = R * translation * pheasant->transform;
    }
}

void TestBoxScene::OnInput(const InputState& input) {
    if (input.rightMouseDown) {
        m_cameraYaw += input.mouseDeltaX * m_mouseSensitivity;
        m_cameraPitch += input.mouseDeltaY * m_mouseSensitivity;
        m_cameraPitch = std::clamp(m_cameraPitch, -1.4f, 1.4f);
    }

    m_cameraDistance -= input.scrollDelta * 2.0f;
    m_cameraDistance = std::clamp(m_cameraDistance, 10.0f, 150.0f);

    constexpr float targetY = 15.0f;

    const float camX = std::sin(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;
    const float camY = std::sin(m_cameraPitch) * m_cameraDistance + targetY;
    const float camZ = std::cos(m_cameraYaw) * std::cos(m_cameraPitch) * m_cameraDistance;

    m_cameraEye = TriVector(camX, camY, camZ);
    m_cameraTarget = TriVector(0.0f, targetY, 0.0f);
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);
}

void TestBoxScene::OnGui() {
    ImGui::Begin("Test Box Scene");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Separator();
    ImGui::Text("Box: 100 x 30 x 40");
    ImGui::Text("4 Planes (bottom, top, left, right)");
    ImGui::Text("2 Spheres (rotating)");
    ImGui::Text("1 Pheasant (sine wave, scale=%.2f)", m_PHEASANT_SCALE);
    ImGui::Text("1 Point Light");
    ImGui::Separator();
    ImGui::Text("Sphere Rotation:");
    ImGui::Text("  Angle: %.2f rad (%.1f deg)", m_rotationAngle, m_rotationAngle * (180.0f / kPi));
    ImGui::SliderFloat("Speed", &m_PHEASANT_ROTATION_SPEED, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::Text("  Right-drag: Orbit camera");
    ImGui::Text("  Scroll: Zoom");
    ImGui::Separator();
    ImGui::Text("Camera Yaw: %.2f", m_cameraYaw);
    ImGui::Text("Camera Pitch: %.2f", m_cameraPitch);
    ImGui::Text("Camera Distance: %.1f", m_cameraDistance);
    ImGui::End();
}

void TestBoxScene::OnShutdown() {}
