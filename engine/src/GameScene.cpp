#include "GameScene.h"
#include "VulkanRenderer.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>

GameScene::GameScene(const std::string& resourceDir)
    : m_resourceDir(resourceDir) {}

void GameScene::OnInit([[maybe_unused]] VulkanRenderer* renderer) {}
void GameScene::OnUpdate([[maybe_unused]] float deltaTime) {}
void GameScene::OnInput([[maybe_unused]] const InputState& input) {}
void GameScene::OnGui() {}
void GameScene::OnShutdown() {}

void GameScene::UpdateFPS(float deltaTime) noexcept {
    m_fpsAccumulator += deltaTime;
    ++m_fpsFrameCount;

    if (m_fpsAccumulator >= 1.0f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }
}

// === Mesh Management ===

uint32_t GameScene::LoadMesh(std::string_view objFilename, std::string_view textureFilename) {
    auto mesh = std::make_unique<Mesh>();
    std::string fullPath = m_resourceDir + "/" + std::string(objFilename);

    if (!mesh->LoadFromFile(fullPath)) {
        throw std::runtime_error("Failed to load mesh: " + fullPath);
    }

    mesh->CenterOnOrigin();
    mesh->BuildBVH();

    if (!textureFilename.empty() && m_textureFilename.empty()) {
        m_textureFilename = m_resourceDir + "/" + std::string(textureFilename);
    }

    if (!textureFilename.empty()) {
        std::string fullTexturePath = m_resourceDir + "/" + std::string(textureFilename);
        for (size_t i = 0; i < mesh->MaterialCount(); ++i) {
            mesh->GetMaterial(i).diffuseTexturePath = fullTexturePath;
            mesh->GetMaterial(i).diffuseTextureIndex = 0;
        }
    }

    const auto meshId = static_cast<uint32_t>(m_meshes.size());
    m_meshes.push_back(std::move(mesh));
    m_meshCPUDataFreed.push_back(false);
    return meshId;
}

void GameScene::FreeMeshCPUData(uint32_t meshId) {
    if (meshId >= m_meshes.size() || !m_meshes[meshId]) {
        return;
    }

    if (!m_meshCPUDataFreed[meshId]) {
        m_meshes[meshId]->ClearCPUData();
        m_meshCPUDataFreed[meshId] = true;
    }
}

void GameScene::FreeAllMeshCPUData() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_meshes.size()); ++i) {
        FreeMeshCPUData(i);
    }
}

bool GameScene::IsMeshCPUDataFreed(uint32_t meshId) const noexcept {
    return meshId < m_meshCPUDataFreed.size() && m_meshCPUDataFreed[meshId];
}

// === Mesh Instance Management ===

uint32_t GameScene::AddMeshInstance(uint32_t meshId, const TriVector& position, std::string_view name) {
    Motor transform(
        1.0f,
        position.e032() * 0.5f,
        position.e013() * 0.5f,
        position.e021() * 0.5f,
        0.0f, 0.0f, 0.0f, 0.0f
    );
    return AddMeshInstance(meshId, transform, name);
}

uint32_t GameScene::AddMeshInstance(uint32_t meshId, const Motor& transform, std::string_view name) {
    if (meshId >= m_meshes.size()) {
        throw std::runtime_error("Invalid mesh ID: " + std::to_string(meshId));
    }

    MeshInstance instance;
    instance.meshId = meshId;
    instance.transform = transform;
    instance.visible = true;
    instance.name = std::string(name);

    const auto instanceId = static_cast<uint32_t>(m_meshInstances.size());
    m_meshInstances.push_back(std::move(instance));

    if (!name.empty()) {
        m_namedInstances[std::string(name)] = instanceId;
    }

    return instanceId;
}

MeshInstance* GameScene::GetInstance(uint32_t instanceId) noexcept {
    return instanceId < m_meshInstances.size() ? &m_meshInstances[instanceId] : nullptr;
}

MeshInstance* GameScene::FindInstance(std::string_view name) noexcept {
    const auto it = m_namedInstances.find(std::string(name));
    return it != m_namedInstances.end() ? &m_meshInstances[it->second] : nullptr;
}

void GameScene::SetInstanceTransform(uint32_t instanceId, const Motor& transform) {
    if (auto* instance = GetInstance(instanceId)) {
        instance->transform = transform;
    }
}

void GameScene::SetInstancePosition(uint32_t instanceId, const TriVector& position) {
    if (auto* instance = GetInstance(instanceId)) {
        instance->transform = Motor(
            1.0f,
            position.e032() * 0.5f,
            position.e013() * 0.5f,
            position.e021() * 0.5f,
            0.0f, 0.0f, 0.0f, 0.0f
        );
    }
}

void GameScene::SetInstanceVisible(uint32_t instanceId, bool visible) {
    if (auto* instance = GetInstance(instanceId)) {
        instance->visible = visible;
    }
}

// === Primitive Management ===

uint32_t GameScene::AddSphere(const TriVector& center, float radius, const Scene::Material& material) {
    return AddSphere(center.e032(), center.e013(), center.e021(), radius, material);
}

uint32_t GameScene::AddPlane(const Vector& plane, const Scene::Material& material) {
    return AddPlane(plane.e1(), plane.e2(), plane.e3(), -plane.e0(), material);
}

uint32_t GameScene::AddSphere(float x, float y, float z, float radius, const Scene::Material& material) {
    auto sphere = Scene::GPUSphere::create(x, y, z, radius, material);
    const auto id = static_cast<uint32_t>(m_sceneData.spheres.size());
    m_sceneData.spheres.push_back(sphere);
    return id;
}

uint32_t GameScene::AddPlane(float nx, float ny, float nz, float distance, const Scene::Material& material) {
    auto plane = Scene::GPUPlane::create(nx, ny, nz, distance, material);
    const auto id = static_cast<uint32_t>(m_sceneData.planes.size());
    m_sceneData.planes.push_back(plane);
    return id;
}

uint32_t GameScene::AddGroundPlane(float height, const Scene::Material& material) {
    return AddPlane(0.0f, 1.0f, 0.0f, height, material);
}

Scene::GPUSphere* GameScene::GetSphere(uint32_t id) noexcept {
    return id < m_sceneData.spheres.size() ? &m_sceneData.spheres[id] : nullptr;
}

Scene::GPUPlane* GameScene::GetPlane(uint32_t id) noexcept {
    return id < m_sceneData.planes.size() ? &m_sceneData.planes[id] : nullptr;
}

void GameScene::SetSphereMaterial(uint32_t id, const Scene::Material& material) {
    if (auto* sphere = GetSphere(id)) {
        sphere->SetMaterial(material);
    }
}

void GameScene::SetPlaneMaterial(uint32_t id, const Scene::Material& material) {
    if (auto* plane = GetPlane(id)) {
        plane->SetMaterial(material);
    }
}

// === Lighting ===

uint32_t GameScene::AddDirectionalLight(const TriVector& direction,
                                         const Scene::Color& color,
                                         float intensity) {
    return AddDirectionalLight(direction.e032(), direction.e013(), direction.e021(), color, intensity);
}

uint32_t GameScene::AddPointLight(const TriVector& position,
                                   const Scene::Color& color,
                                   float intensity,
                                   float range) {
    return AddPointLight(position.e032(), position.e013(), position.e021(), color, intensity, range);
}

uint32_t GameScene::AddSpotLight(const TriVector& position,
                                  const TriVector& direction,
                                  const Scene::Color& color,
                                  float intensity,
                                  float angle,
                                  float range) {
    return AddSpotLight(position.e032(), position.e013(), position.e021(),
                        direction.e032(), direction.e013(), direction.e021(),
                        color, intensity, angle, range);
}

uint32_t GameScene::AddDirectionalLight(float dx, float dy, float dz,
                                         const Scene::Color& color,
                                         float intensity) {
    m_sceneData.lights.push_back(Scene::GPULight::Directional(dx, dy, dz, color, intensity));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

uint32_t GameScene::AddPointLight(float x, float y, float z,
                                   const Scene::Color& color,
                                   float intensity,
                                   float range) {
    m_sceneData.lights.push_back(Scene::GPULight::Point(x, y, z, color, intensity, range));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

uint32_t GameScene::AddSpotLight(float px, float py, float pz,
                                  float dx, float dy, float dz,
                                  const Scene::Color& color,
                                  float intensity,
                                  float angle,
                                  float range) {
    m_sceneData.lights.push_back(Scene::GPULight::Spot(px, py, pz, dx, dy, dz, color, intensity, range, angle, 0.1f));
    return static_cast<uint32_t>(m_sceneData.lights.size() - 1);
}

Scene::GPULight* GameScene::GetLight(uint32_t id) noexcept {
    return id < m_sceneData.lights.size() ? &m_sceneData.lights[id] : nullptr;
}

// === Debug Visualization ===

void GameScene::DrawDebugLine(const TriVector& from, const TriVector& to, const Scene::Color& color) {
    DrawDebugLine(from.e032(), from.e013(), from.e021(),
                  to.e032(), to.e013(), to.e021(), color);
}

void GameScene::DrawDebugLine(float x1, float y1, float z1, float x2, float y2, float z2,
                               const Scene::Color& color) {
    if (!m_debugDrawEnabled) return;

    DebugLine line{};
    line.from[0] = x1; line.from[1] = y1; line.from[2] = z1;
    line.to[0] = x2; line.to[1] = y2; line.to[2] = z2;
    line.color[0] = color.r; line.color[1] = color.g; line.color[2] = color.b;
    m_debugLines.push_back(line);
}

void GameScene::DrawDebugPoint(const TriVector& point, float size, const Scene::Color& color) {
    if (point.e123() != 0.0f)
    {
        TriVector drawPoint = point.Normalized();
        const float x = drawPoint.e032();
        const float y = drawPoint.e013();
        const float z = drawPoint.e021();

        // Draw as a small cross
        DrawDebugLine(x - size, y, z, x + size, y, z, color);
        DrawDebugLine(x, y - size, z, x, y + size, z, color);
        DrawDebugLine(x, y, z - size, x, y, z + size, color);
    }
}

void GameScene::DrawDebugAxes(const TriVector& position, float size) {
    const float x = position.e032();
    const float y = position.e013();
    const float z = position.e021();

    DrawDebugLine(x, y, z, x + size, y, z, Scene::Color::Red());   // X axis
    DrawDebugLine(x, y, z, x, y + size, z, Scene::Color::Green()); // Y axis
    DrawDebugLine(x, y, z, x, y, z + size, Scene::Color::Blue());  // Z axis
}

void GameScene::DrawDebugAxes(const Motor& transform, float size) {
    // Extract translation from motor
    const float tx = 2.0f * transform.e01();
    const float ty = 2.0f * transform.e02();
    const float tz = 2.0f * transform.e03();

    DrawDebugAxes(TriVector(tx, ty, tz), size);
}

void GameScene::DrawDebugLine(const BiVector& line, float halfLength, const Scene::Color& color) {
    // In PGA, a BiVector line has:
    // - Direction: d = (e23, e31, e12)
    // - Moment: m = (e01, e02, e03)
    // A point on the line: p = d × m / |d|²

    const float dx = line.e23();
    const float dy = line.e31();
    const float dz = line.e12();

    const float mx = line.e01();
    const float my = line.e02();
    const float mz = line.e03();

    // Check if direction is non-zero (not an ideal line)
    const float dirLenSq = dx * dx + dy * dy + dz * dz;
    if (dirLenSq < 1e-10f) {
        return; // Cannot visualize ideal lines
    }

    // Normalize direction
    const float invDirLen = 1.0f / std::sqrt(dirLenSq);
    const float ndx = dx * invDirLen;
    const float ndy = dy * invDirLen;
    const float ndz = dz * invDirLen;

    // Compute a point on the line: p = (d × m) / |d|²
    // Cross product d × m
    const float px = (dy * mz - dz * my) / dirLenSq;
    const float py = (dz * mx - dx * mz) / dirLenSq;
    const float pz = (dx * my - dy * mx) / dirLenSq;

    // Draw line from p - halfLength*d to p + halfLength*d
    DrawDebugLine(
        px - halfLength * ndx, py - halfLength * ndy, pz - halfLength * ndz,
        px + halfLength * ndx, py + halfLength * ndy, pz + halfLength * ndz,
        color
    );
}

void GameScene::ClearDebugDraw() {
    m_debugLines.clear();
}

bool GameScene::ProjectToScreen(const TriVector& worldPos, float& screenX, float& screenY) const {
    // Get camera vectors
    const float eyeX = m_cameraEye.e032();
    const float eyeY = m_cameraEye.e013();
    const float eyeZ = m_cameraEye.e021();

    const float targetX = m_cameraTarget.e032();
    const float targetY = m_cameraTarget.e013();
    const float targetZ = m_cameraTarget.e021();

    // Camera forward
    float fwdX = targetX - eyeX;
    float fwdY = targetY - eyeY;
    float fwdZ = targetZ - eyeZ;
    const float fwdLen = std::sqrt(fwdX * fwdX + fwdY * fwdY + fwdZ * fwdZ);
    if (fwdLen < 0.0001f) return false;
    fwdX /= fwdLen; fwdY /= fwdLen; fwdZ /= fwdLen;

    // Camera up
    const float upX = m_cameraUp.e032();
    const float upY = m_cameraUp.e013();
    const float upZ = m_cameraUp.e021();

    // Camera right = forward x up
    float rightX = fwdY * upZ - fwdZ * upY;
    float rightY = fwdZ * upX - fwdX * upZ;
    float rightZ = fwdX * upY - fwdY * upX;
    const float rightLen = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (rightLen < 0.0001f) return false;
    rightX /= rightLen; rightY /= rightLen; rightZ /= rightLen;

    // Recalculate up = right x forward
    const float camUpX = rightY * fwdZ - rightZ * fwdY;
    const float camUpY = rightZ * fwdX - rightX * fwdZ;
    const float camUpZ = rightX * fwdY - rightY * fwdX;

    // World position relative to camera
    const float px = worldPos.e032() - eyeX;
    const float py = worldPos.e013() - eyeY;
    const float pz = worldPos.e021() - eyeZ;

    // Transform to camera space
    const float camX = px * rightX + py * rightY + pz * rightZ;
    const float camY = px * camUpX + py * camUpY + pz * camUpZ;
    const float camZ = px * fwdX + py * fwdY + pz * fwdZ;

    // Check if behind camera
    if (camZ <= 0.01f) return false;

    // Perspective projection
    const float fovRad = m_cameraFov * 3.14159265f / 180.0f;
    const float tanHalfFov = std::tan(fovRad * 0.5f);
    const float aspect = static_cast<float>(m_screenWidth) / static_cast<float>(m_screenHeight);

    const float ndcX = camX / (camZ * tanHalfFov * aspect);
    const float ndcY = camY / (camZ * tanHalfFov);

    // Check if outside view frustum
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) return false;

    // Convert to screen coordinates
    screenX = (ndcX + 1.0f) * 0.5f * static_cast<float>(m_screenWidth);
    screenY = (1.0f - ndcY) * 0.5f * static_cast<float>(m_screenHeight);

    return true;
}

void GameScene::RenderDebugDraw() const {
    if (!m_debugDrawEnabled || m_debugLines.empty()) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    for (const auto& line : m_debugLines) {
        float x1, y1, x2, y2;
        const TriVector from(line.from[0], line.from[1], line.from[2]);
        const TriVector to(line.to[0], line.to[1], line.to[2]);

        const bool fromVisible = ProjectToScreen(from, x1, y1);
        const bool toVisible = ProjectToScreen(to, x2, y2);

        if (fromVisible && toVisible) {
            const ImU32 color = IM_COL32(
                static_cast<int>(line.color[0] * 255.0f),
                static_cast<int>(line.color[1] * 255.0f),
                static_cast<int>(line.color[2] * 255.0f),
                255
            );
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, 2.0f);
        }
    }
}
