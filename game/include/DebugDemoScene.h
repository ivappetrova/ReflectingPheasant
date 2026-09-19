#pragma once

#include "GameScene.h"
#include <cstdint>

// Demo scene showcasing debug visualization features
class DebugDemoScene final : public GameScene {
public:
    explicit DebugDemoScene(const std::string& resourceDir);

    void OnInit(VulkanRenderer* renderer) override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const InputState& input) override;
    void OnGui() override;
    void OnShutdown() override;

private:
    void Reset();

    // Camera orbit
    float m_cameraYaw{0.5f};
    float m_cameraPitch{0.4f};
    float m_cameraDistance{25.0f};
    float m_mouseSensitivity{0.005f};

    // Animation
    float m_time{0.0f};
    float m_PHEASANT_ROTATION_SPEED{1.0f};
    bool m_paused{false};
    const TriVector m_sphereStartPosition{0.0f, 3.0f, 0.0f, 1.0f};

    // Sphere IDs for animation
    uint32_t m_sphereId{0};

    // Debug visualization toggles
    bool m_showAxes{true};
    bool m_showGrid{true};
    bool m_showLines{true};

    // Previous key states for edge detection
    bool m_prevKeyP{false};
    bool m_prevKey1{false};
    bool m_prevKey2{false};
    bool m_prevKey3{false};
    bool m_prevKeyR{false};
};
