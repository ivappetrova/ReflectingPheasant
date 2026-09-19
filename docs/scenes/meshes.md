# Meshes

FlyTracer supports loading OBJ mesh files with optional textures. Meshes use BVH acceleration for efficient ray intersection.

## Loading Meshes

Place your `.obj` and texture files in the `game/resources/` directory.

```cpp
// Load mesh with texture
uint32_t meshId = LoadMesh("model.obj", "texture.png");

// Load mesh without texture
uint32_t meshId = LoadMesh("model.obj");
```

!!! note "Supported Formats"
    - **Meshes**: Wavefront OBJ (`.obj`)
    - **Textures**: PNG, JPG, BMP, TGA (via stb_image)

## Creating Instances

A mesh can have multiple instances in the scene, each with its own transform.

```cpp
// Place at position (using TriVector)
uint32_t instanceId = AddMeshInstance(meshId, TriVector(x, y, z), "optionalName");

// Place with full Motor transform
Motor transform = Motor::Translation(x, y, z) * Motor::Rotation(45.0f, BiVector::Y());
uint32_t instanceId = AddMeshInstance(meshId, transform, "rotatedObject");
```

## Modifying Instances

### By ID

```cpp
MeshInstance* instance = GetInstance(instanceId);

if (instance) {
    instance->scale = 2.0f;           // Uniform scale
    instance->visible = false;        // Hide instance
    instance->transform = newMotor;   // Update transform
}
```

### By Name

```cpp
MeshInstance* instance = FindInstance("pheasant");

if (instance) {
    instance->scale = 0.5f;
}
```

### Convenience Methods

```cpp
// Update transform
SetInstanceTransform(instanceId, newMotor);

// Update position only (resets rotation)
SetInstancePosition(instanceId, TriVector(newX, newY, newZ));

// Toggle visibility
SetInstanceVisible(instanceId, false);
```

## MeshInstance Structure

```cpp
struct MeshInstance {
    uint32_t meshId;      // Reference to loaded mesh
    Motor transform;      // Position and rotation
    float scale;          // Uniform scale factor
    bool visible;         // Render flag
    std::string name;     // Optional identifier
};
```

## Memory Management

Mesh CPU data can be freed after upload to GPU to save memory:

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    m_meshId = LoadMesh("large_model.obj", "texture.png");
    AddMeshInstance(m_meshId, TriVector(0.0f, 0.0f, 0.0f));

    // Free CPU-side vertex/triangle data (mesh is already on GPU)
    FreeMeshCPUData(m_meshId);

    // Or free all meshes at once
    FreeAllMeshCPUData();
}

// Check if data was freed
bool freed = IsMeshCPUDataFreed(m_meshId);
```

!!! warning
    After freeing CPU data, you cannot create new instances of that mesh or modify its geometry.

## Example: Textured Model

```cpp
class ModelScene : public GameScene {
    uint32_t m_meshId{0};
    uint32_t m_instanceId{0};
    float m_rotation{0.0f};

public:
    explicit ModelScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

    void OnInit(VulkanRenderer* renderer) override {
        // Load textured mesh
        m_meshId = LoadMesh("pheasant.obj", "pheasant.png");

        // Create instance with name
        m_instanceId = AddMeshInstance(m_meshId, TriVector(0.0f, 0.0f, 0.0f), "bird");

        // Scale it down
        if (auto* inst = FindInstance("bird")) {
            inst->scale = 0.5f;
        }

        // Environment
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f), Scene::Color::White(), 2.0f, 50.0f);

        m_cameraEye = TriVector(0.0f, 3.0f, 8.0f);
        m_cameraTarget = TriVector(0.0f, 1.0f, 0.0f);
    }

    void OnUpdate(float deltaTime) override {
        m_rotation += deltaTime * 0.5f;

        // Rotate the model
        BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        Motor rotation = Motor::Rotation(m_rotation * 57.3f, yAxis);  // rad to deg

        SetInstanceTransform(m_instanceId, rotation);
    }
};
```

## Example: Multiple Instances

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Load mesh once
    uint32_t treeId = LoadMesh("tree.obj", "tree.png");

    // Create a forest of instances
    for (int x = -5; x <= 5; x += 2) {
        for (int z = -5; z <= 5; z += 2) {
            float offsetX = (rand() % 100) / 100.0f - 0.5f;
            float offsetZ = (rand() % 100) / 100.0f - 0.5f;
            float scale = 0.8f + (rand() % 100) / 250.0f;

            auto instanceId = AddMeshInstance(treeId,
                TriVector(x + offsetX, 0.0f, z + offsetZ));

            if (auto* inst = GetInstance(instanceId)) {
                inst->scale = scale;
            }
        }
    }

    // Free CPU data - all instances share the same GPU mesh
    FreeMeshCPUData(treeId);
}
```

## OBJ File Requirements

For best results, ensure your OBJ files:

- Have **triangulated** faces (no quads or n-gons)
- Include **vertex normals** (`vn` lines)
- Include **texture coordinates** (`vt` lines) if using textures
- Use a **single material** per mesh (multi-material not fully supported)

!!! tip "Blender Export Settings"
    When exporting from Blender:

    - Enable "Triangulate Faces"
    - Enable "Write Normals"
    - Enable "Include UVs"
