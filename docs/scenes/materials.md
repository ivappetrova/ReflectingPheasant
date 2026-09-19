# Materials

FlyTracer supports multiple shading models, from simple flat shading to physically-based rendering (PBR).

## Shading Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| **Flat** | Solid color, no shading | Debug, stylized |
| **Lambert** | Diffuse-only shading | Matte surfaces |
| **Phong** | Diffuse + specular highlights | Plastic, shiny surfaces |
| **PBR** | Physically-based rendering | Realistic materials |

## Creating Materials

### Flat

No lighting calculations - just a solid color.

```cpp
auto mat = Scene::Material::Flat(Scene::Color::Red());
```

### Lambert

Diffuse shading based on surface normal and light direction.

```cpp
auto mat = Scene::Material::Lambert(Scene::Color(0.8f, 0.6f, 0.4f));
```

### Phong

Adds specular highlights to Lambert shading.

```cpp
// Color and shininess (higher = sharper highlights)
auto mat = Scene::Material::Phong(Scene::Color::White(), 32.0f);

// With reflectivity (0.0 - 1.0)
auto mat = Scene::Material::Phong(Scene::Color::White(), 64.0f, 0.5f);
```

### PBR (Physically-Based Rendering)

Modern material model with metalness and roughness parameters.

```cpp
// Basic PBR: color, metalness, roughness
auto mat = Scene::Material::PBR(Scene::Color::Gold(), 1.0f, 0.3f);

// Metalness: 0.0 = dielectric (plastic), 1.0 = metal
// Roughness: 0.0 = mirror-smooth, 1.0 = completely rough
```

## Convenience Constructors

```cpp
// Metal: high metalness, configurable roughness
auto chrome = Scene::Material::Metal(Scene::Color::White(), 0.1f);  // Shiny chrome
auto brushed = Scene::Material::Metal(Scene::Color::Gray(), 0.4f);  // Brushed metal

// Matte: zero metalness, full roughness
auto chalk = Scene::Material::Matte(Scene::Color::White());

// Glossy: zero metalness, low roughness (shiny plastic)
auto plastic = Scene::Material::Glossy(Scene::Color::Red(), 0.2f);
```

## Colors

Use `Scene::Color` for RGB values (0.0 - 1.0 range):

```cpp
// Custom color
Scene::Color custom(0.8f, 0.4f, 0.2f);  // RGB

// Predefined colors
Scene::Color::White()
Scene::Color::Black()
Scene::Color::Red()
Scene::Color::Green()
Scene::Color::Blue()
Scene::Color::Yellow()
Scene::Color::Cyan()
Scene::Color::Magenta()
Scene::Color::Gray(0.5f)  // Grayscale with intensity
```

## Material Properties

The full `Scene::Material` struct:

```cpp
struct Material {
    ShadingMode shadingMode;    // Flat, Lambert, Phong, or PBR
    Color color;                // Base color
    float shininess;            // Phong exponent (default: 32)
    float metalness;            // PBR metalness (0-1)
    float roughness;            // PBR roughness (0-1)
    float reflectivity;         // Mirror reflectivity (0-1)
};
```

## Examples

### Material Showcase

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray(0.5f)));

    // Row 1: Shading modes
    AddSphere(TriVector(-6.0f, 1.5f, 0.0f), 1.5f,
              Scene::Material::Flat(Scene::Color::Red()));
    AddSphere(TriVector(-2.0f, 1.5f, 0.0f), 1.5f,
              Scene::Material::Lambert(Scene::Color::Red()));
    AddSphere(TriVector(2.0f, 1.5f, 0.0f), 1.5f,
              Scene::Material::Phong(Scene::Color::Red(), 32.0f));
    AddSphere(TriVector(6.0f, 1.5f, 0.0f), 1.5f,
              Scene::Material::PBR(Scene::Color::Red(), 0.0f, 0.3f));

    // Row 2: PBR roughness variation
    for (int i = 0; i < 5; ++i) {
        float roughness = i / 4.0f;  // 0.0 to 1.0
        AddSphere(TriVector(-4.0f + i * 2.0f, 1.5f, 4.0f), 1.0f,
                  Scene::Material::PBR(Scene::Color::White(), 0.0f, roughness));
    }

    // Row 3: PBR metalness variation
    for (int i = 0; i < 5; ++i) {
        float metalness = i / 4.0f;  // 0.0 to 1.0
        AddSphere(TriVector(-4.0f + i * 2.0f, 1.5f, 8.0f), 1.0f,
                  Scene::Material::PBR(Scene::Color(0.9f, 0.7f, 0.3f), metalness, 0.3f));
    }

    AddPointLight(TriVector(0.0f, 10.0f, 4.0f), Scene::Color::White(), 2.0f, 50.0f);
}
```

### Realistic Materials

```cpp
// Gold
auto gold = Scene::Material::Metal(Scene::Color(1.0f, 0.84f, 0.0f), 0.3f);

// Copper
auto copper = Scene::Material::Metal(Scene::Color(0.72f, 0.45f, 0.2f), 0.4f);

// Chrome
auto chrome = Scene::Material::Metal(Scene::Color(0.95f, 0.95f, 0.95f), 0.05f);

// Rubber
auto rubber = Scene::Material::Matte(Scene::Color(0.1f, 0.1f, 0.1f));

// Plastic
auto plastic = Scene::Material::Glossy(Scene::Color::Red(), 0.15f);

// Glass-like (no refraction, just reflection)
auto glass = Scene::Material::Phong(Scene::Color(0.9f, 0.9f, 1.0f), 128.0f, 0.9f);
```

### Changing Materials at Runtime

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Animate roughness
    float roughness = (std::sin(m_time) + 1.0f) * 0.5f;  // 0 to 1
    SetSphereMaterial(m_sphereId,
                      Scene::Material::PBR(Scene::Color::Gold(), 1.0f, roughness));
}
```
