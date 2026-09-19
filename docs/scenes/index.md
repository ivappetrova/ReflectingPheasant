# Creating Scenes

This section covers everything you need to build raytraced scenes in FlyTracer.

## Topics

- [**Overview**](overview.md) - Understand the GameScene lifecycle and architecture
- [**Primitives**](primitives.md) - Add spheres and infinite planes to your scene
- [**Materials**](materials.md) - Configure Flat, Lambert, Phong, and PBR shading
- [**Meshes**](meshes.md) - Load OBJ models with textures
- [**Lighting**](lighting.md) - Point, directional, and spot lights
- [**Camera**](camera.md) - Eye-target-up camera setup and controls
- [**Animation**](animation.md) - Transform objects with PGA Motors
- [**Input Handling**](input.md) - Handle mouse and keyboard input
- [**Debug UI**](imgui.md) - Create ImGui debug interfaces
- [**Debug Visualization**](debug.md) - Draw debug lines, points, and axes

## Minimal Example

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

    void OnInit(VulkanRenderer* renderer) override {
        // Ground
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));

        // Sphere
        AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
                  Scene::Material::Metal(Scene::Color::Red(), 0.3f));

        // Light
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                      Scene::Color::White(), 1.5f, 50.0f);

        // Camera
        m_cameraEye = TriVector(0.0f, 5.0f, 12.0f);
        m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);
    }
};
```
