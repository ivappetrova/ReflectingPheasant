#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <vector>
#include <string>
#include <memory>

#include "Scene.h"
#include "FlyFish.h"
#include "GameScene.h"  // For MeshInstance definition
#include "VulkanRenderer.h"  // Vulkan rendering abstraction

class Mesh;

class Application {
public:
    Application(int width, int height, const std::string& title,
                std::unique_ptr<GameScene> scene,
                const std::string& shaderDir = "shaders");
    ~Application() noexcept;

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

    // Scene management
    void setScene(const Scene::SceneData& scene);
    Scene::SceneData& getScene() { return m_sceneData; }
    const Scene::SceneData& getScene() const { return m_sceneData; }

    // Camera control using three PGA points (TriVectors)
    // eye: camera position, target: look-at point, up: point above eye defining up direction
    void setCamera(const TriVector& eye, const TriVector& target, const TriVector& up);

    // Multi-mesh support for GameScene
    void uploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes);

private:
    // Initialization
    void initSDL();

    // Main loop
    void handleEvents();
    void processInput(float deltaTime);
    void update(float deltaTime);
    void render();

    // Cleanup
    void cleanup();

    // Window and basic properties
    SDL_Window* m_window{nullptr};
    const int m_width;
    const int m_height;
    const std::string m_title;
    const std::string m_shaderDir;
    bool m_running{false};
    bool m_windowMinimized{false};

    // Vulkan rendering (all Vulkan code now in VulkanRenderer)
    std::unique_ptr<VulkanRenderer> m_renderer;

    // Scene data (owned by GameScene, cached here for rendering)
    Scene::SceneData m_sceneData;
    std::vector<MeshInstance> m_meshInstances;

    // Game scene
    std::unique_ptr<GameScene> m_gameScene;

    // Animation state
    float m_time{0.0f};
    uint32_t m_frameCount{0};

    // Camera state (three PGA points) - managed by GameScene, cached here for rendering
    TriVector m_cameraEye{0.0f, 3.5f, 8.0f};     // Camera position
    TriVector m_cameraTarget{0.0f, 1.5f, 0.0f};  // Look-at point
    TriVector m_cameraUp{0.0f, 4.5f, 8.0f};      // Point above eye (defines up direction)

    // Input state - passed to GameScene for handling
    bool m_rightMouseDown{false};
    bool m_middleMouseDown{false};
    bool m_leftMouseDown{false};
    float m_lastMouseX{0.0f};
    float m_lastMouseY{0.0f};
    float m_mouseDeltaX{0.0f};
    float m_mouseDeltaY{0.0f};
    float m_scrollDelta{0.0f};
    bool m_keyW{false}, m_keyA{false}, m_keyS{false}, m_keyD{false};
    bool m_keyQ{false}, m_keyE{false};  // Up/down movement
    bool m_keyUp{false}, m_keyDown{false}, m_keyLeft{false}, m_keyRight{false};  // Arrow keys

    // Modifier keys
    bool m_keySpace{false};
    bool m_keyShift{false};
    bool m_keyCtrl{false};
    bool m_keyAlt{false};

    // Number keys (for mode switching, object selection)
    bool m_key1{false}, m_key2{false}, m_key3{false}, m_key4{false}, m_key5{false};
    bool m_key6{false}, m_key7{false}, m_key8{false}, m_key9{false}, m_key0{false};

    // Action keys
    bool m_keyR{false};   // Reset
    bool m_keyP{false};   // Pause
    bool m_keyG{false};   // Toggle gravity
    bool m_keyV{false};   // Toggle velocity visualization
    bool m_keyF{false};   // Toggle fullscreen / freeze
    bool m_keyT{false};   // Toggle something
    bool m_keyEsc{false}; // Escape (handled differently, but tracked)

    // FPS tracking
    float m_fps{0.0f};
    float m_fpsAccumulator{0.0f};
    uint32_t m_fpsFrameCount{0};

    // Camera rotation matrix (row-major 3x3) and FOV for rendering
    // Row 0: right vector, Row 1: up vector, Row 2: forward vector (-Z)
    std::array<float, 9> m_cameraRotation{1, 0, 0, 0, 1, 0, 0, 0, 1};  // Identity rotation
    float m_cameraFov{45.0f};
};
