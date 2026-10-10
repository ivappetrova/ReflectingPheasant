# Reflecting Pheasant

An interactive scene rendered with a **real-time Vulkan compute-shader raytracer**, where all movement and mirroring is done with **Projective Geometric Algebra (PGA)** instead of matrices.

Made for the Geometric Algebra (GEOA) course at Howest DAE, on top of the university's FlyTracer engine and the FlyFish PGA math library.

<!-- TODO: add a screenshot or GIF, e.g. ![Reflecting Pheasant](docs/screenshot.png) -->

## The idea

Every transformation in the scene is a PGA *motor* (a combined rotation + translation) applied with the **sandwich product** `M * P * ~M`. The project is built around four formulas:

| # | Formula | Used for |
|---|---------|----------|
| 1 | Translation using motors | Player movement |
| 2 | Rotation around an axis using bivector motors | Updating the mini sphere |
| 3 | Orbiting with `R * T` | Pheasant movement |
| 4 | Reflection using the sandwich product | The pheasant's reflection |

### Translation (player movement)

The translation motor stores half of the displacement, hence the `0.5` compensation factor.

```cpp
const float SANDWICH_COMPENSATION{ 0.5f };
Motor T(1.0f,
        worldX * step * SANDWICH_COMPENSATION, 0.0f, worldZ * step * SANDWICH_COMPENSATION,
        0.0f, 0.0f, 0.0f, 0.0f);

TriVector newPlayerPos = (T * m_PlayerCenter * ~T).Grade3();
```

### Orbiting (`R * T`)

Each frame the pheasant's transform is multiplied with a small rotation motor around the Y axis, which makes it orbit.

```cpp
const BiVector Y_AXIS(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
const Motor R{ Motor::Rotation(INCREMENT_DEGREES, Y_AXIS) };
const Motor T{ pheasant->transform };

pheasant->transform = R * T;
```

### Reflection

The pheasant's position is reflected with a sandwich product, and the reflected position is turned back into a translation motor for rendering.

```cpp
TriVector reflected = (m_REFLECTOR_POS * m_PheasantPos * ~m_REFLECTOR_POS).Grade3();
m_PheasantPos = reflected;
```

## Engine features (FlyTracer)

- Vulkan 1.3 compute shader raytracing
- Flat, Lambert, Phong and PBR materials
- BVH acceleration for ray-mesh intersection
- OBJ loading (tinyobjloader) with diffuse textures
- Spheres, planes and triangle meshes
- ImGui debug overlay
- macOS (MoltenVK) and Windows

## Controls

<!-- TODO: list your keys / mouse bindings -->

## Building

Requirements: CMake 3.20+, a C++23 compiler (Clang, GCC or MSVC) and the Vulkan SDK (with `glslc`).

```bash
git clone --recurse-submodules https://github.com/ivappetrova/ReflectingPheasant.git
cd ReflectingPheasant

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/bin/FlyTracer
```

Visual Studio: open the folder with CMake support, or run `cmake -B build -G "Visual Studio 17 2022"`.

## Project structure

```
engine/    FlyTracer engine (headers, sources, GLSL compute shaders)
game/      the Reflecting Pheasant scene, resources and config
docs/      documentation
```

## Credits

- **Engine:** FlyTracer, provided by Howest DAE. Per the engine's own README, most of its code was generated with Claude Opus 4.5.
- **PGA math:** FlyFish library.
- **Libraries:** SDL3, Dear ImGui, tinyobjloader, Google Test.
- **Gameplay and geometric algebra:** Iva Petrova.
