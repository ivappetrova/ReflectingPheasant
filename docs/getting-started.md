# Getting Started

## Requirements

- **CMake** 3.20 or higher
- **C++23 compiler** - Clang, GCC 13+, or MSVC 2022
- **Vulkan SDK** - With `glslc` shader compiler

## Building

### Command Line

```bash
# Clone the repository
git clone --recursive https://github.com/fdlombae/FlyTracer.git
cd FlyTracer

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/bin/FlyTracer
```

### Visual Studio

Open the folder with CMake support, or generate a solution:

```bash
cmake -B build -G "Visual Studio 17 2022"
```

Then open `build/FlyTracer.sln`.

### CLion

Open the project folder directly. CLion will detect the CMakeLists.txt automatically.

## Project Structure

```
FlyTracer/
├── engine/
│   ├── include/       # Engine headers (GameScene.h, Scene.h, etc.)
│   ├── src/           # Engine implementation
│   └── shaders/       # GLSL compute shaders
├── game/
│   ├── include/       # Your scene headers
│   ├── src/           # Your scene implementation + main.cpp
│   └── resources/     # Models, textures, config files
├── external/
│   └── FlyFish/       # PGA math library
└── tests/             # Unit tests
```

## Creating Your First Scene

1. Create a new header in `game/include/`:

```cpp
// game/include/MyScene.h
#pragma once
#include "GameScene.h"

class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir);

    void OnInit(VulkanRenderer* renderer) override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const InputState& input) override;
    void OnGui() override;
};
```

2. Implement it in `game/src/`:

```cpp
// game/src/MyScene.cpp
#include "MyScene.h"
#include <imgui.h>

MyScene::MyScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void MyScene::OnInit(VulkanRenderer* renderer) {
    // Add scene objects here
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
    AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
              Scene::Material::PBR(Scene::Color::Red()));
    AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                  Scene::Color::White(), 1.5f, 50.0f);

    m_cameraEye = TriVector(0.0f, 5.0f, 15.0f);
    m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);
}

void MyScene::OnUpdate(float deltaTime) {
    // Animation logic here
}

void MyScene::OnInput(const InputState& input) {
    // Input handling here
}

void MyScene::OnGui() {
    ImGui::Begin("My Scene");
    ImGui::Text("Hello, FlyTracer!");
    ImGui::End();
}
```

3. Update `main.cpp` to use your scene:

```cpp
auto scene = std::make_unique<MyScene>(config.resourceDirectory);
```

4. Add your `.cpp` file to `CMakeLists.txt`:

```cmake
add_executable(${PROJECT_NAME}
    game/src/main.cpp
    game/src/MyScene.cpp
)
```

5. Build and run!


## Next Steps

- [Scene Overview](scenes/overview.md) - Understand the GameScene architecture
- [Primitives](scenes/primitives.md) - Add spheres and planes
- [Materials](scenes/materials.md) - Configure shading and colors
- [Meshes](scenes/meshes.md) - Load OBJ models with textures
