#pragma once

#include "Scene.h"
#include "FlyFish.h"
#include "Mesh.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cmath>

class VulkanRenderer;

// ============================================================================
// Debug drawing primitives
// ============================================================================
struct DebugLine {
    float from[3];
    float to[3];
    float color[3];
};

struct DebugSphere {
    float center[3];
    float radius;
    float color[3];
};

struct MeshInstance {
    uint32_t meshId;
    Motor transform;
    float scale{1.0f};
    bool visible{true};
    std::string name;
};

// Input state passed to OnInput
struct InputState {
    // Mouse state
    bool leftMouseDown{false};
    bool rightMouseDown{false};
    bool middleMouseDown{false};
    float mouseX{0.0f};
    float mouseY{0.0f};
    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
    float scrollDelta{0.0f};

    // Movement keys
    bool keyW{false}, keyA{false}, keyS{false}, keyD{false};
    bool keyQ{false}, keyE{false};
    bool keyUp{false}, keyDown{false}, keyLeft{false}, keyRight{false};

    // Modifier keys
    bool keySpace{false};
    bool keyShift{false};
    bool keyCtrl{false};
    bool keyAlt{false};

    // Number keys (for mode switching, object selection)
    bool key1{false}, key2{false}, key3{false}, key4{false}, key5{false};
    bool key6{false}, key7{false}, key8{false}, key9{false}, key0{false};

    // Common action keys
    bool keyR{false};  // Reset
    bool keyP{false};  // Pause
    bool keyG{false};  // Toggle gravity
    bool keyV{false};  // Toggle velocity visualization
    bool keyF{false};  // Toggle fullscreen / freeze
    bool keyT{false};  // Toggle something
    bool keyEsc{false}; // Escape

    float deltaTime{0.0f};
};

class GameScene {
public:
    explicit GameScene(const std::string& resourceDir);
    virtual ~GameScene() = default;

    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;
    GameScene(GameScene&&) = default;
    GameScene& operator=(GameScene&&) = default;

    virtual void OnInit(VulkanRenderer* renderer);
    virtual void OnUpdate(float deltaTime);
    virtual void OnInput(const InputState& input);
    virtual void OnGui();
    virtual void OnShutdown();

    [[nodiscard]] Scene::SceneData& GetSceneData() noexcept { return m_sceneData; }
    [[nodiscard]] const Scene::SceneData& GetSceneData() const noexcept { return m_sceneData; }

    [[nodiscard]] const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const noexcept { return m_meshes; }
    [[nodiscard]] std::vector<std::unique_ptr<Mesh>>& GetMeshes() noexcept { return m_meshes; }
    [[nodiscard]] const std::vector<MeshInstance>& GetMeshInstances() const noexcept { return m_meshInstances; }
    [[nodiscard]] std::vector<MeshInstance>& GetMeshInstances() noexcept { return m_meshInstances; }

    [[nodiscard]] Mesh* GetMesh(uint32_t id) noexcept {
        return id < m_meshes.size() ? m_meshes[id].get() : nullptr;
    }
    [[nodiscard]] const Mesh* GetMesh(uint32_t id) const noexcept {
        return id < m_meshes.size() ? m_meshes[id].get() : nullptr;
    }

    [[nodiscard]] const TriVector& GetCameraEye() const noexcept { return m_cameraEye; }
    [[nodiscard]] const TriVector& GetCameraTarget() const noexcept { return m_cameraTarget; }
    [[nodiscard]] const TriVector& GetCameraUp() const noexcept { return m_cameraUp; }
    [[nodiscard]] float GetCameraFov() const noexcept { return m_cameraFov; }

    void SetCamera(const TriVector& eye, const TriVector& target, const TriVector& up) noexcept {
        m_cameraEye = eye;
        m_cameraTarget = target;
        m_cameraUp = up;
    }

    void SetCameraFov(float fov) noexcept { m_cameraFov = fov; }

    [[nodiscard]] float GetFPS() const noexcept { return m_fps; }
    void UpdateFPS(float deltaTime) noexcept;

    [[nodiscard]] const std::string& GetResourceDir() const noexcept { return m_resourceDir; }
    [[nodiscard]] const std::string& GetTextureFilename() const noexcept { return m_textureFilename; }

    // Debug drawing (public for Application to call automatically)
    void ClearDebugDraw();
    void RenderDebugDraw() const;
    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }

protected:
    uint32_t LoadMesh(std::string_view objFilename, std::string_view textureFilename = {});

    void FreeMeshCPUData(uint32_t meshId);
    void FreeAllMeshCPUData();
    [[nodiscard]] bool IsMeshCPUDataFreed(uint32_t meshId) const noexcept;

    uint32_t AddMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name = {});
    uint32_t AddMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name = {});

    [[nodiscard]] MeshInstance* GetInstance(uint32_t instanceId) noexcept;
    [[nodiscard]] MeshInstance* FindInstance(std::string_view name) noexcept;
    void SetInstanceTransform(uint32_t instanceId, const Motor& transform);
    void SetInstancePosition(uint32_t instanceId, const TriVector& position);
    void SetInstanceVisible(uint32_t instanceId, bool visible);

    uint32_t AddSphere(const TriVector& center, float radius, const Scene::Material& material);
    uint32_t AddPlane(const Vector& plane, const Scene::Material& material);
    uint32_t AddSphere(float x, float y, float z, float radius, const Scene::Material& material);
    uint32_t AddPlane(float nx, float ny, float nz, float distance, const Scene::Material& material);
    uint32_t AddGroundPlane(float height, const Scene::Material& material);
    [[nodiscard]] Scene::GPUSphere* GetSphere(uint32_t id) noexcept;
    [[nodiscard]] Scene::GPUPlane* GetPlane(uint32_t id) noexcept;

    void SetSphereMaterial(uint32_t id, const Scene::Material& material);
    void SetPlaneMaterial(uint32_t id, const Scene::Material& material);

    uint32_t AddDirectionalLight(const TriVector& direction, const Scene::Color& color, float intensity);
    uint32_t AddPointLight(const TriVector& position, const Scene::Color& color, float intensity, float range);
    uint32_t AddSpotLight(const TriVector& position, const TriVector& direction,
                          const Scene::Color& color, float intensity, float angle, float range);
    uint32_t AddDirectionalLight(float dx, float dy, float dz, const Scene::Color& color, float intensity);
    uint32_t AddPointLight(float x, float y, float z, const Scene::Color& color, float intensity, float range);
    uint32_t AddSpotLight(float px, float py, float pz, float dx, float dy, float dz,
                          const Scene::Color& color, float intensity, float angle, float range);
    [[nodiscard]] Scene::GPULight* GetLight(uint32_t id) noexcept;

    // ========================================================================
    // Debug visualization
    // ========================================================================

    // Draw debug primitives (rendered as ImGui overlay)
    void DrawDebugLine(const TriVector& from, const TriVector& to,
                       const Scene::Color& color = Scene::Color::White());
    void DrawDebugLine(float x1, float y1, float z1, float x2, float y2, float z2,
                       const Scene::Color& color = Scene::Color::White());

    // Draw a point as a small cross
    void DrawDebugPoint(const TriVector& point, float size = 0.1f,
                        const Scene::Color& color = Scene::Color::White());

    // Draw coordinate axes at a transform
    void DrawDebugAxes(const TriVector& position, float size = 1.0f);
    void DrawDebugAxes(const Motor& transform, float size = 1.0f);

    // Draw a line from a BiVector (Plücker line representation)
    // Creates two points along the line extending 'halfLength' in each direction
    void DrawDebugLine(const BiVector& line, float halfLength = 10.0f,
                       const Scene::Color& color = Scene::Color::White());

    std::string m_resourceDir;
    Scene::SceneData m_sceneData;

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<MeshInstance> m_meshInstances;
    std::unordered_map<std::string, uint32_t> m_namedInstances;
    std::vector<bool> m_meshCPUDataFreed;
    std::string m_textureFilename;

    TriVector m_cameraEye{0.0f, 3.5f, 8.0f};
    TriVector m_cameraTarget{0.0f, 1.5f, 0.0f};
    TriVector m_cameraUp{0.0f, 1.0f, 0.0f};
    float m_cameraFov{45.0f};

    float m_fps{0.0f};
    float m_fpsAccumulator{0.0f};
    uint32_t m_fpsFrameCount{0};

    // Debug drawing
    std::vector<DebugLine> m_debugLines;
    bool m_debugDrawEnabled{true};

    // Screen dimensions for debug projection (set by Application)
    int m_screenWidth{1280};
    int m_screenHeight{720};

    // Helper for 3D to 2D projection
    bool ProjectToScreen(const TriVector& worldPos, float& screenX, float& screenY) const;
};
