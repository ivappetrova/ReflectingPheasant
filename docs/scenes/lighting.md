# Lighting

FlyTracer supports three light types: **point lights**, **directional lights**, and **spot lights**.

## Point Lights

Emit light in all directions from a single point. Intensity falls off with distance.

```cpp
uint32_t id = AddPointLight(
    TriVector(x, y, z),     // Position
    Scene::Color::White(),  // Color
    1.5f,                   // Intensity
    50.0f                   // Range (falloff distance)
);

// Or using float coordinates
uint32_t id = AddPointLight(x, y, z, color, intensity, range);
```

### Example

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Warm overhead light
    AddPointLight(TriVector(0.0f, 10.0f, 0.0f),
                  Scene::Color(1.0f, 0.95f, 0.8f), 2.0f, 100.0f);

    // Blue accent light
    AddPointLight(TriVector(-5.0f, 3.0f, 5.0f),
                  Scene::Color(0.4f, 0.6f, 1.0f), 1.0f, 30.0f);

    // Red accent light
    AddPointLight(TriVector(5.0f, 3.0f, 5.0f),
                  Scene::Color(1.0f, 0.4f, 0.4f), 1.0f, 30.0f);
}
```

## Directional Lights

Parallel light rays from an infinitely distant source (like the sun). No position, only direction.

```cpp
uint32_t id = AddDirectionalLight(
    TriVector(dx, dy, dz),  // Direction (will be normalized)
    Scene::Color::White(),  // Color
    1.0f                    // Intensity
);

// Or using float coordinates
uint32_t id = AddDirectionalLight(dx, dy, dz, color, intensity);
```

### Example

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Sunlight from upper-right
    AddDirectionalLight(TriVector(-0.5f, -1.0f, -0.3f),
                        Scene::Color(1.0f, 0.98f, 0.9f), 1.2f);

    // Soft fill light from opposite direction
    AddDirectionalLight(TriVector(0.3f, -0.5f, 0.2f),
                        Scene::Color(0.6f, 0.7f, 0.9f), 0.3f);
}
```

## Spot Lights

Cone-shaped light from a point in a specific direction.

```cpp
uint32_t id = AddSpotLight(
    TriVector(px, py, pz),  // Position
    TriVector(dx, dy, dz),  // Direction
    Scene::Color::White(),  // Color
    2.0f,                   // Intensity
    30.0f,                  // Cone angle (degrees)
    50.0f                   // Range
);

// Or using float coordinates
uint32_t id = AddSpotLight(px, py, pz, dx, dy, dz, color, intensity, angle, range);
```

### Example

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Spotlight pointing down at the center
    AddSpotLight(
        TriVector(0.0f, 15.0f, 0.0f),   // Above the scene
        TriVector(0.0f, -1.0f, 0.0f),   // Pointing down
        Scene::Color::White(),
        3.0f,   // Bright
        25.0f,  // 25-degree cone
        30.0f   // Range
    );
}
```

## Modifying Lights

```cpp
Scene::GPULight* light = GetLight(lightId);

if (light) {
    // Change position (point/spot lights)
    light->position[0] = newX;
    light->position[1] = newY;
    light->position[2] = newZ;

    // Change color
    light->color[0] = 1.0f;
    light->color[1] = 0.5f;
    light->color[2] = 0.0f;

    // Change intensity
    light->intensity = 2.5f;
}
```

## Light Properties

The `GPULight` structure:

```cpp
struct GPULight {
    float position[3];   // World position (point/spot)
    int32_t type;        // 0=directional, 1=point, 2=spot

    float direction[3];  // Light direction (directional/spot)
    float intensity;     // Brightness multiplier

    float color[3];      // RGB color
    float range;         // Falloff distance (point/spot)

    float angle;         // Cone angle in radians (spot)
    // ... padding
};
```

## Lighting Recipes

### Indoor Scene

```cpp
void SetupIndoorLighting() {
    // Main ceiling light
    AddPointLight(TriVector(0.0f, 9.0f, 0.0f),
                  Scene::Color(1.0f, 0.95f, 0.85f), 2.0f, 20.0f);

    // Window light (directional, cooler)
    AddDirectionalLight(TriVector(1.0f, -0.5f, 0.0f),
                        Scene::Color(0.8f, 0.9f, 1.0f), 0.8f);

    // Ambient fill (very soft)
    AddDirectionalLight(TriVector(0.0f, -1.0f, 0.0f),
                        Scene::Color(0.5f, 0.5f, 0.6f), 0.2f);
}
```

### Outdoor Scene

```cpp
void SetupOutdoorLighting() {
    // Sun
    AddDirectionalLight(TriVector(-0.3f, -1.0f, -0.5f),
                        Scene::Color(1.0f, 0.98f, 0.92f), 1.5f);

    // Sky fill (soft blue from above)
    AddDirectionalLight(TriVector(0.0f, -1.0f, 0.0f),
                        Scene::Color(0.6f, 0.75f, 1.0f), 0.4f);

    // Ground bounce (subtle warm from below)
    AddDirectionalLight(TriVector(0.0f, 1.0f, 0.0f),
                        Scene::Color(0.4f, 0.35f, 0.3f), 0.15f);
}
```

### Dramatic Spotlight

```cpp
void SetupDramaticLighting() {
    // Single dramatic spotlight
    AddSpotLight(
        TriVector(5.0f, 10.0f, 5.0f),
        TriVector(-0.4f, -0.8f, -0.4f),  // Angled down
        Scene::Color(1.0f, 0.9f, 0.8f),
        4.0f,   // High intensity
        20.0f,  // Tight cone
        25.0f
    );

    // Subtle fill to prevent pure black shadows
    AddDirectionalLight(TriVector(0.0f, -1.0f, 0.0f),
                        Scene::Color(0.2f, 0.2f, 0.3f), 0.1f);
}
```

### Animated Light

```cpp
class AnimatedLightScene : public GameScene {
    uint32_t m_lightId{0};
    float m_time{0.0f};

    void OnInit(VulkanRenderer* renderer) override {
        m_lightId = AddPointLight(TriVector(0.0f, 5.0f, 0.0f),
                                  Scene::Color::White(), 1.5f, 30.0f);
    }

    void OnUpdate(float deltaTime) override {
        m_time += deltaTime;

        if (auto* light = GetLight(m_lightId)) {
            // Pulsing intensity
            light->intensity = 1.5f + std::sin(m_time * 3.0f) * 0.5f;
        }
    }
};
```
