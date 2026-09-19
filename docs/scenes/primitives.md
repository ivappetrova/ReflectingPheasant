# Primitives

FlyTracer supports two primitive types: **spheres** and **planes**. Both are raytraced analytically for perfect precision.

## Spheres

Spheres are defined by a center point and radius.

### Adding Spheres

```cpp
// Using PGA TriVector for position
uint32_t id = AddSphere(TriVector(x, y, z), radius, material);

// Using float coordinates
uint32_t id = AddSphere(x, y, z, radius, material);
```

### Example

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Red metallic sphere
    m_redSphere = AddSphere(TriVector(-3.0f, 2.0f, 0.0f), 2.0f,
                            Scene::Material::Metal(Scene::Color::Red(), 0.2f));

    // Blue glossy sphere
    m_blueSphere = AddSphere(TriVector(3.0f, 1.5f, 0.0f), 1.5f,
                             Scene::Material::Glossy(Scene::Color::Blue(), 0.1f));

    // White matte sphere
    AddSphere(0.0f, 4.0f, -2.0f, 1.0f,
              Scene::Material::Matte(Scene::Color::White()));
}
```

### Modifying Spheres

After adding a sphere, you can modify it using the returned ID:

```cpp
// Get sphere pointer
Scene::GPUSphere* sphere = GetSphere(sphereId);

if (sphere) {
    // Move the sphere
    sphere->SetCenter(TriVector(newX, newY, newZ));

    // Change radius (requires re-upload, not recommended for animation)
    sphere->radius = 3.0f;
}

// Change material
SetSphereMaterial(sphereId, Scene::Material::PBR(Scene::Color::Green(), 0.5f, 0.3f));
```

## Planes

Planes are infinite surfaces defined using PGA Vectors (homogeneous coordinates).

### Plane Representation

A plane in PGA is represented as a Vector with components `(d, nx, ny, nz)` where:

- `(nx, ny, nz)` is the normal direction
- `d` is the distance from origin along the normal

!!! note "Plane Equation"
    The plane equation is: `nx*x + ny*y + nz*z = d`

### Adding Planes

```cpp
// Using PGA Vector: Vector(d, nx, ny, nz)
uint32_t id = AddPlane(Vector(distance, nx, ny, nz), material);

// Using components directly
uint32_t id = AddPlane(nx, ny, nz, distance, material);

// Convenience: horizontal ground plane at height y
uint32_t id = AddGroundPlane(height, material);
```

### Example: Cornell Box

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Floor at y=0 (normal pointing up)
    AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.7f, 0.7f, 0.7f)));

    // Or use the convenience function
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));

    // Ceiling at y=10 (normal pointing down)
    AddPlane(Vector(10.0f, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.9f, 0.9f)));

    // Left wall at x=-5 (normal pointing right, red)
    AddPlane(Vector(5.0f, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.2f, 0.2f)));

    // Right wall at x=5 (normal pointing left, green)
    AddPlane(Vector(5.0f, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.2f, 0.9f, 0.2f)));

    // Back wall at z=-5 (normal pointing forward)
    AddPlane(Vector(5.0f, 0.0f, 0.0f, 1.0f),
             Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.8f)));
}
```

### Modifying Planes

```cpp
Scene::GPUPlane* plane = GetPlane(planeId);

if (plane) {
    // Planes are typically static, but you can modify them
    plane->normal[0] = 0.0f;
    plane->normal[1] = 1.0f;
    plane->normal[2] = 0.0f;
    plane->distance = 5.0f;
}

// Change material
SetPlaneMaterial(planeId, Scene::Material::Phong(Scene::Color::Blue(), 64.0f));
```

## Common Patterns

### Animated Sphere

```cpp
class MyScene : public GameScene {
    uint32_t m_sphereId{0};
    float m_time{0.0f};

    void OnInit(VulkanRenderer* renderer) override {
        m_sphereId = AddSphere(TriVector(0.0f, 2.0f, 0.0f), 1.0f,
                               Scene::Material::Metal(Scene::Color::Gold()));
    }

    void OnUpdate(float deltaTime) override {
        m_time += deltaTime;

        if (auto* sphere = GetSphere(m_sphereId)) {
            float y = 2.0f + std::sin(m_time * 2.0f);
            sphere->SetCenter(TriVector(0.0f, y, 0.0f));
        }
    }
};
```

### Room Setup

```cpp
void SetupRoom(float width, float height, float depth) {
    float hw = width / 2.0f;
    float hd = depth / 2.0f;

    // Floor and ceiling
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray(0.7f)));
    AddPlane(Vector(height, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color::Gray(0.9f)));

    // Walls
    AddPlane(Vector(hw, 1.0f, 0.0f, 0.0f),   // Left
             Scene::Material::Lambert(Scene::Color(0.8f, 0.3f, 0.3f)));
    AddPlane(Vector(hw, -1.0f, 0.0f, 0.0f),  // Right
             Scene::Material::Lambert(Scene::Color(0.3f, 0.8f, 0.3f)));
    AddPlane(Vector(hd, 0.0f, 0.0f, 1.0f),   // Back
             Scene::Material::Lambert(Scene::Color::Gray(0.8f)));
}
```
