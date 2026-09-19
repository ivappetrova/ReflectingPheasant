# FlyTracer

A real-time **Vulkan compute shader raytracer** built for educational purposes. Uses Projective Geometric Algebra (PGA) via the [FlyFish](https://github.com/fdlombae/FlyFish) library for 3D transformations.

## Features

- **Vulkan 1.3 Compute Shader Raytracing** - Real-time raytracing on the GPU
- **Multiple Shading Models** - Flat, Lambert, Phong, and PBR materials
- **BVH Acceleration** - Bounding Volume Hierarchy for efficient ray-mesh intersection
- **OBJ Mesh Loading** - Load 3D models with textures
- **Scene Primitives** - Spheres and infinite planes
- **Multiple Light Types** - Point, directional, and spot lights
- **PGA Transformations** - Motors for rotations and translations

## Quick Example

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

    void OnInit(VulkanRenderer* renderer) override {
        // Add a ground plane
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color(0.5f, 0.5f, 0.5f)));

        // Add a red metallic sphere
        AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
                  Scene::Material::Metal(Scene::Color(0.9f, 0.2f, 0.2f)));

        // Add a point light
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                      Scene::Color::White(), 1.5f, 50.0f);

        // Set camera
        m_cameraEye = TriVector(0.0f, 5.0f, 15.0f);
        m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);
    }
};
```

## Getting Started

Check out the [Getting Started](getting-started.md) guide to build and run FlyTracer, then explore the [Creating Scenes](scenes/overview.md) documentation to build your own raytraced scenes.
