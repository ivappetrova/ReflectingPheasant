# Debug Visualization

FlyTracer provides debug drawing functions to visualize points, lines, and coordinate axes as an ImGui overlay on top of the raytraced scene.

## Setup

Debug drawing is enabled by default. Call drawing functions in `OnUpdate` and render them in `OnGui`:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Draw debug primitives here
    DrawDebugLine(TriVector(0, 0, 0), TriVector(5, 5, 0), Scene::Color::Red());
}

void MyScene::OnGui() {
    // Your ImGui code...

    // Render all debug primitives
    RenderDebugDraw();
}
```

## Lines

Draw lines between two points in 3D space.

```cpp
// Using TriVectors
DrawDebugLine(TriVector(x1, y1, z1), TriVector(x2, y2, z2), color);

// Using float coordinates
DrawDebugLine(x1, y1, z1, x2, y2, z2, color);
```

### Example: Grid

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const float gridSize = 10.0f;
    const float gridStep = 2.0f;
    const Scene::Color gridColor(0.5f, 0.5f, 0.5f);

    // Draw grid lines along X axis
    for (float x = -gridSize; x <= gridSize; x += gridStep) {
        DrawDebugLine(x, 0.01f, -gridSize, x, 0.01f, gridSize, gridColor);
    }
    // Draw grid lines along Z axis
    for (float z = -gridSize; z <= gridSize; z += gridStep) {
        DrawDebugLine(-gridSize, 0.01f, z, gridSize, 0.01f, z, gridColor);
    }
}
```

## Points

Draw points as small 3D crosses.

```cpp
DrawDebugPoint(TriVector(x, y, z, 1.0f), size, color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `point` | `TriVector` | Position (w component should be 1.0) |
| `size` | `float` | Cross size (default: 0.1) |
| `color` | `Scene::Color` | Point color (default: white) |

### Example

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Mark sphere positions
    DrawDebugPoint(TriVector(5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Cyan());
    DrawDebugPoint(TriVector(-5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Magenta());
}
```

## Coordinate Axes

Draw RGB coordinate axes (X=red, Y=green, Z=blue) at a position or transform.

```cpp
// At a position
DrawDebugAxes(TriVector(x, y, z), size);

// At a Motor transform
DrawDebugAxes(motor, size);
```

### Example

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // World origin axes
    DrawDebugAxes(TriVector(0.0f, 0.0f, 0.0f), 5.0f);

    // Axes at an object's position
    if (auto* sphere = GetSphere(m_sphereId)) {
        TriVector pos(sphere->center[0], sphere->center[1], sphere->center[2]);
        DrawDebugAxes(pos, 1.0f);
    }
}
```

## BiVector Lines (PGA)

Draw lines from a BiVector (Plücker line representation). Useful for visualizing PGA line elements.

```cpp
DrawDebugLine(BiVector line, float halfLength, Scene::Color color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `line` | `BiVector` | Plücker line (e.g., from regressive product of two points) |
| `halfLength` | `float` | How far to extend the line in each direction (default: 10.0) |
| `color` | `Scene::Color` | Line color (default: white) |

### Example: Lines from Points

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Create a line from two points using the regressive product
    TriVector p1(0.0f, 2.0f, 0.0f, 1.0f);
    TriVector p2(5.0f, 4.0f, 5.0f, 1.0f);
    BiVector line = p1 & p2;  // Join operation

    DrawDebugLine(line, 8.0f, Scene::Color::Yellow());
}
```

### Example: Rotating Line

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Create a line
    TriVector pA(0.0f, 5.0f, 0.0f, 1.0f);
    TriVector pB(3.0f, 5.0f, 3.0f, 1.0f);
    BiVector line = pA & pB;

    // Rotate it using a Motor
    Motor R(std::cos(m_time), 0, 0, 0, 0, std::sin(m_time), 0, 0);
    BiVector rotatedLine = (R * line * ~R).Grade2();

    DrawDebugLine(rotatedLine, 6.0f, Scene::Color::Cyan());
}
```

## Rendering

Debug primitives are collected during `OnUpdate` and rendered as an ImGui overlay. Call `RenderDebugDraw()` at the end of `OnGui`:

```cpp
void MyScene::OnGui() {
    ImGui::Begin("Debug Panel");
    ImGui::Checkbox("Show Axes", &m_showAxes);
    ImGui::Checkbox("Show Grid", &m_showGrid);
    ImGui::End();

    // Always call at the end of OnGui
    RenderDebugDraw();
}
```

The debug draw list is automatically cleared each frame before `OnUpdate`.

## Toggling Visibility

Use member variables to conditionally draw debug elements:

```cpp
class MyScene : public GameScene {
    bool m_showAxes{true};
    bool m_showGrid{true};

    void OnUpdate(float deltaTime) override {
        if (m_showAxes) {
            DrawDebugAxes(TriVector(0, 0, 0), 5.0f);
        }
        if (m_showGrid) {
            // Draw grid...
        }
    }

    void OnInput(const InputState& input) override {
        // Toggle with number keys
        if (input.key1 && !m_prevKey1) m_showAxes = !m_showAxes;
        if (input.key2 && !m_prevKey2) m_showGrid = !m_showGrid;
        m_prevKey1 = input.key1;
        m_prevKey2 = input.key2;
    }
};
```
