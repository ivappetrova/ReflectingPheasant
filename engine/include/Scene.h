#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include "FlyFish.h"

namespace Scene {

// ============================================================================
// Color - Simple RGB color struct
// ============================================================================
struct Color {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};

    constexpr Color() noexcept = default;
    constexpr Color(float r_, float g_, float b_) noexcept : b(r_), g(g_), r(b_) {}

    static constexpr Color White() noexcept { return {1.0f, 1.0f, 1.0f}; }
    static constexpr Color Black() noexcept { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Color Red() noexcept { return {1.0f, 0.0f, 0.0f}; }
    static constexpr Color Green() noexcept { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Color Blue() noexcept { return {0.0f, 0.0f, 1.0f}; }
    static constexpr Color Yellow() noexcept { return {1.0f, 1.0f, 0.0f}; }
    static constexpr Color Cyan() noexcept { return {0.0f, 1.0f, 1.0f}; }
    static constexpr Color Magenta() noexcept { return {1.0f, 0.0f, 1.0f}; }
    static constexpr Color Gray(float v = 0.5f) noexcept { return {v, v, v}; }
};

// ============================================================================
// Shading modes for materials
// ============================================================================
enum class ShadingMode : int32_t {
    Flat = 0,
    Lambert = 1,
    Phong = 2,
    PBR = 3
};

// ============================================================================
// Material - Unified material definition
// ============================================================================
struct Material {
    ShadingMode shadingMode{ShadingMode::Lambert};
    Color color{1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float metalness{0.0f};
    float roughness{0.5f};
    float reflectivity{0.0f};

    static constexpr Material Flat(const Color& col) noexcept {
        return {ShadingMode::Flat, col, 32.0f, 0.0f, 0.5f, 0.0f};
    }

    static constexpr Material Lambert(const Color& col) noexcept {
        return {ShadingMode::Lambert, col, 32.0f, 0.0f, 0.5f, 0.0f};
    }

    static constexpr Material Phong(const Color& col, float shine = 32.0f, float reflect = 0.0f) noexcept {
        return {ShadingMode::Phong, col, shine, 0.0f, 0.5f, reflect};
    }

    static constexpr Material PBR(const Color& col, float metal = 0.0f, float rough = 0.5f) noexcept {
        return {ShadingMode::PBR, col, 32.0f, metal, rough, 0.0f};
    }

    static constexpr Material Metal(const Color& col, float rough = 0.3f) noexcept {
        return PBR(col, 1.0f, rough);
    }

    static constexpr Material Matte(const Color& col) noexcept {
        return PBR(col, 0.0f, 1.0f);
    }

    static constexpr Material Glossy(const Color& col, float rough = 0.2f) noexcept {
        return PBR(col, 0.0f, rough);
    }
};

// ============================================================================
// Light types
// ============================================================================
enum class LightType : int32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

// ============================================================================
// GPU Sphere structure (48 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) GPUSphere {
    float center[3]{0, 0, 0};
    float radius{1.0f};
    float color[3]{1, 1, 1};
    float shininess{32.0f};
    float reflectivity{0.0f};
    int32_t shadingMode{static_cast<int32_t>(ShadingMode::PBR)};
    float metalness{0.0f};
    float roughness{0.5f};

    [[nodiscard]] static constexpr GPUSphere create(
        float cx, float cy, float cz, float r,
        const Material& mat) noexcept {
        GPUSphere sphere{};
        sphere.center[0] = cx;
        sphere.center[1] = cy;
        sphere.center[2] = cz;
        sphere.radius = r;
        sphere.color[0] = mat.color.r;
        sphere.color[1] = mat.color.g;
        sphere.color[2] = mat.color.b;
        sphere.shininess = mat.shininess;
        sphere.reflectivity = mat.reflectivity;
        sphere.shadingMode = static_cast<int32_t>(mat.shadingMode);
        sphere.metalness = mat.metalness;
        sphere.roughness = mat.roughness;
        return sphere;
    }

    [[nodiscard]] static GPUSphere create(
        const TriVector& pt, float r,
        const Material& mat) noexcept {
        const float w = pt.e123();
        float x, y, z;
        if (std::abs(w) > 1e-6f) {
            x = pt.e032() / w;
            y = pt.e013() / w;
            z = pt.e021() / w;
        } else {
            x = pt.e032();
            y = pt.e013();
            z = pt.e021();
        }
        return create(x, y, z, r, mat);
    }

    [[nodiscard]] TriVector GetCenter() const noexcept {
        return TriVector(center[0], center[1], center[2], 1.0f);
    }

    void SetCenter(const TriVector& pt) noexcept {
        const float w = pt.e123();
        if (std::abs(w) > 1e-6f) {
            center[0] = pt.e032() / w;
            center[1] = pt.e013() / w;
            center[2] = pt.e021() / w;
        } else {
            center[0] = pt.e032();
            center[1] = pt.e013();
            center[2] = pt.e021();
        }
    }

    [[nodiscard]] Material GetMaterial() const noexcept {
        Material m;
        m.shadingMode = static_cast<ShadingMode>(shadingMode);
        m.color = Color(color[0], color[1], color[2]);
        m.shininess = shininess;
        m.reflectivity = reflectivity;
        m.metalness = metalness;
        m.roughness = roughness;
        return m;
    }

    void SetMaterial(const Material& mat) noexcept {
        color[0] = mat.color.r;
        color[1] = mat.color.g;
        color[2] = mat.color.b;
        shininess = mat.shininess;
        reflectivity = mat.reflectivity;
        shadingMode = static_cast<int32_t>(mat.shadingMode);
        metalness = mat.metalness;
        roughness = mat.roughness;
    }
};

// ============================================================================
// GPU Plane structure (48 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) GPUPlane {
    float normal[3]{0, 1, 0};
    float distance{0.0f};
    float color[3]{0.5f, 0.5f, 0.5f};
    float _padding{0.0f};
    float reflectivity{0.0f};
    int32_t shadingMode{static_cast<int32_t>(ShadingMode::Lambert)};
    float metalness{0.0f};
    float roughness{0.5f};

    [[nodiscard]] static constexpr GPUPlane create(
        float nx, float ny, float nz, float dist,
        const Material& mat) noexcept {
        GPUPlane plane{};
        plane.normal[0] = nx;
        plane.normal[1] = ny;
        plane.normal[2] = nz;
        plane.distance = dist;
        plane.color[0] = mat.color.r;
        plane.color[1] = mat.color.g;
        plane.color[2] = mat.color.b;
        plane.reflectivity = mat.reflectivity;
        plane.shadingMode = static_cast<int32_t>(mat.shadingMode);
        plane.metalness = mat.metalness;
        plane.roughness = mat.roughness;
        return plane;
    }

    [[nodiscard]] static GPUPlane create(
        const Vector& planeVec,
        const Material& mat) noexcept {
        return create(planeVec.e1(), planeVec.e2(), planeVec.e3(), -planeVec.e0(), mat);
    }

    [[nodiscard]] static constexpr GPUPlane ground(float height, const Material& mat) noexcept {
        return create(0.0f, 1.0f, 0.0f, height, mat);
    }

    [[nodiscard]] Vector GetPlane() const noexcept {
        return Vector(-distance, normal[0], normal[1], normal[2]);
    }

    void SetPlane(const Vector& v) noexcept {
        normal[0] = v.e1();
        normal[1] = v.e2();
        normal[2] = v.e3();
        distance = -v.e0();
    }

    [[nodiscard]] Material GetMaterial() const noexcept {
        Material m;
        m.shadingMode = static_cast<ShadingMode>(shadingMode);
        m.color = Color(color[0], color[1], color[2]);
        m.reflectivity = reflectivity;
        m.metalness = metalness;
        m.roughness = roughness;
        return m;
    }

    void SetMaterial(const Material& mat) noexcept {
        color[0] = mat.color.r;
        color[1] = mat.color.g;
        color[2] = mat.color.b;
        reflectivity = mat.reflectivity;
        shadingMode = static_cast<int32_t>(mat.shadingMode);
        metalness = mat.metalness;
        roughness = mat.roughness;
    }
};

// ============================================================================
// GPU Light structure (64 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) GPULight {
    float position[3]{0, 0, 0};
    float typeAsFloat{0};
    float direction[3]{0, -1, 0};
    float range{10.0f};
    float color[3]{1, 1, 1};
    float intensity{1.0f};
    float spotAngle{0.5f};
    float spotSoftness{0.2f};
    float _pad[2]{0, 0};

    [[nodiscard]] static constexpr GPULight Directional(
        float dx, float dy, float dz,
        const Color& col,
        float inten = 1.0f) noexcept {
        GPULight light{};
        light.typeAsFloat = static_cast<float>(static_cast<int32_t>(LightType::Directional));
        light.direction[0] = dx;
        light.direction[1] = dy;
        light.direction[2] = dz;
        light.color[0] = col.r;
        light.color[1] = col.g;
        light.color[2] = col.b;
        light.intensity = inten;
        return light;
    }

    [[nodiscard]] static constexpr GPULight Point(
        float px, float py, float pz,
        const Color& col,
        float inten = 1.0f, float rng = 10.0f) noexcept {
        GPULight light{};
        light.typeAsFloat = static_cast<float>(static_cast<int32_t>(LightType::Point));
        light.position[0] = px;
        light.position[1] = py;
        light.position[2] = pz;
        light.color[0] = col.r;
        light.color[1] = col.g;
        light.color[2] = col.b;
        light.intensity = inten;
        light.range = rng;
        return light;
    }

    [[nodiscard]] static constexpr GPULight Spot(
        float px, float py, float pz,
        float dx, float dy, float dz,
        const Color& col,
        float inten = 1.0f, float rng = 10.0f,
        float angle = 0.5f, float soft = 0.2f) noexcept {
        GPULight light{};
        light.typeAsFloat = static_cast<float>(static_cast<int32_t>(LightType::Spot));
        light.position[0] = px;
        light.position[1] = py;
        light.position[2] = pz;
        light.direction[0] = dx;
        light.direction[1] = dy;
        light.direction[2] = dz;
        light.color[0] = col.r;
        light.color[1] = col.g;
        light.color[2] = col.b;
        light.intensity = inten;
        light.range = rng;
        light.spotAngle = angle;
        light.spotSoftness = soft;
        return light;
    }
};

// ============================================================================
// GPU BVH Node structure (32 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) BVHNode {
    float minBounds[3]{1e30f, 1e30f, 1e30f};
    int32_t leftFirst{0};
    float maxBounds[3]{-1e30f, -1e30f, -1e30f};
    int32_t triCount{0};
};

// ============================================================================
// GPU Material structure for meshes (32 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) GPUMaterial {
    float diffuse[3]{0.8f, 0.8f, 0.8f};
    float shininess{32.0f};
    int32_t shadingMode{static_cast<int32_t>(ShadingMode::PBR)};
    int32_t diffuseTextureIndex{-1};
    float metalness{0.0f};
    float roughness{0.5f};
};

// ============================================================================
// GPU Texture Info structure - per-texture metadata for multi-texture support (16 bytes)
// ============================================================================
struct alignas(16) GPUTextureInfo {
    uint32_t offset{0};     // Offset into texture data buffer (in vec4 pixels)
    uint32_t width{1};      // Texture width
    uint32_t height{1};     // Texture height
    uint32_t _pad{0};       // Padding to 16 bytes
};

// ============================================================================
// GPU Mesh Info structure - per-mesh offsets for multi-mesh support (32 bytes)
// ============================================================================
struct alignas(16) GPUMeshInfo {
    uint32_t vertexOffset{0};      // Offset into vertex buffer
    uint32_t triangleOffset{0};    // Offset into triangle buffer
    uint32_t bvhNodeOffset{0};     // Offset into BVH node buffer
    uint32_t bvhTriIdxOffset{0};   // Offset into BVH triangle index buffer
    uint32_t triangleCount{0};     // Number of triangles in this mesh
    uint32_t bvhNodeCount{0};      // Number of BVH nodes in this mesh
    uint32_t _pad[2]{0, 0};        // Padding to 32 bytes
};

// ============================================================================
// GPU Mesh Instance structure (144 bytes, 16-byte aligned)
// ============================================================================
struct alignas(16) GPUMeshInstance {
    float transform[16];
    float invTransform[16];
    uint32_t meshId{0};
    uint32_t visible{1};
    float _pad[2]{0, 0};

    [[nodiscard]] static GPUMeshInstance identity(uint32_t mesh = 0) noexcept {
        GPUMeshInstance inst{};
        for (int i = 0; i < 16; ++i) {
            inst.transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
            inst.invTransform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
        inst.meshId = mesh;
        inst.visible = 1;
        return inst;
    }

    [[nodiscard]] static GPUMeshInstance translation(float x, float y, float z, uint32_t mesh = 0) noexcept {
        GPUMeshInstance inst = identity(mesh);
        inst.transform[12] = x;
        inst.transform[13] = y;
        inst.transform[14] = z;
        inst.invTransform[12] = -x;
        inst.invTransform[13] = -y;
        inst.invTransform[14] = -z;
        return inst;
    }
};

// ============================================================================
// Scene data container
// ============================================================================
struct SceneData {
    std::vector<GPUSphere> spheres;
    std::vector<GPUPlane> planes;
    std::vector<GPULight> lights;
    std::vector<GPUMaterial> materials;

    void Clear() noexcept {
        spheres.clear();
        planes.clear();
        lights.clear();
        materials.clear();
    }

    [[nodiscard]] static SceneData createDefaultScene() {
        SceneData scene;

        scene.spheres.push_back(GPUSphere::create(-2.0f, 0.5f, 0.0f, 0.5f,
            Material::PBR(Color(0.8f, 0.2f, 0.2f), 0.0f, 0.3f)));
        scene.spheres.push_back(GPUSphere::create(2.0f, 0.8f, 1.0f, 0.8f,
            Material::Metal(Color(0.2f, 0.8f, 0.2f), 0.2f)));
        scene.spheres.push_back(GPUSphere::create(0.0f, 1.5f, -2.0f, 0.6f,
            Material::Glossy(Color(0.2f, 0.2f, 0.9f), 0.1f)));

        scene.planes.push_back(GPUPlane::ground(0.0f,
            Material::Lambert(Color::Gray(0.5f))));

        scene.lights.push_back(GPULight::Directional(0.577f, 0.577f, 0.289f,
            Color(1.0f, 0.98f, 0.9f), 0.8f));
        scene.lights.push_back(GPULight::Point(-3.0f, 2.0f, 2.0f,
            Color(1.0f, 0.6f, 0.3f), 1.5f, 8.0f));
        scene.lights.push_back(GPULight::Point(3.0f, 1.5f, 1.0f,
            Color(0.4f, 0.6f, 1.0f), 1.2f, 6.0f));
        scene.lights.push_back(GPULight::Spot(0.0f, 4.0f, 0.0f, 0.0f, -1.0f, 0.0f,
            Color(1.0f, 1.0f, 0.8f), 2.0f, 10.0f, 0.5f, 0.2f));

        return scene;
    }
};

} // namespace Scene
