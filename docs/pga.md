# PGA Basics

FlyTracer uses **Projective Geometric Algebra (PGA)** for 3D transformations via the [FlyFish](https://github.com/fdlombae/FlyFish) library. This page covers the essentials you need for scene creation.

## Why PGA?

PGA provides elegant representations for:

- **Points** as TriVectors (trivectors)
- **Planes** as Vectors
- **Lines** as BiVectors (bivectors)
- **Transformations** as Motors (combined rotation + translation)

All transformations use the same "sandwich product" pattern: `M * object * ~M`

## Core Types

### TriVector (Points)

Points in 3D space are represented as **TriVectors** with homogeneous coordinates.

```cpp
// Create a point at (x, y, z)
TriVector point(x, y, z);

// Access coordinates
float x = point.e032();  // x coordinate
float y = point.e013();  // y coordinate
float z = point.e021();  // z coordinate
```

!!! note "Coordinate Names"
    The names `e032`, `e013`, `e021` come from the PGA basis elements. Just remember:

    - `e032()` → X
    - `e013()` → Y
    - `e021()` → Z

### Vector (Planes)

Planes are represented as **Vectors** with components `(d, nx, ny, nz)`.

```cpp
// Plane: nx*x + ny*y + nz*z = d
// Vector(d, nx, ny, nz)

// Floor plane (y = 0, normal pointing up)
Vector floor(0.0f, 0.0f, 1.0f, 0.0f);

// Wall at x = 5 (normal pointing in -X direction)
Vector wall(5.0f, -1.0f, 0.0f, 0.0f);

// Ceiling at y = 10 (normal pointing down)
Vector ceiling(10.0f, 0.0f, -1.0f, 0.0f);
```

### BiVector (Axes/Lines)

BiVectors represent lines and rotation axes. For rotations, you typically use axis-aligned bivectors:

```cpp
// Rotation axes (moment part only, no position offset)
BiVector xAxis(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);  // X axis
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);  // Y axis
BiVector zAxis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);  // Z axis
```

### Motor (Transformations)

Motors combine rotation and translation into a single transformation.

```cpp
// Identity motor (no transformation)
Motor M;

// Translation only
Motor T = Motor::Translation(dx, dy, dz);

// Rotation only (angle in degrees)
BiVector axis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);  // Y axis
Motor R = Motor::Rotation(45.0f, axis);

// Combined: rotate then translate
Motor combined = T * R;  // R applied first, then T
```

## Transforming Objects

All transformations use the **sandwich product**: `M * object * ~M`

The `~M` is the **reverse** of the motor (conjugate).

### Transforming Points

```cpp
// Create a point
TriVector point(1.0f, 2.0f, 3.0f);

// Create a rotation around Y axis
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
Motor R = Motor::Rotation(90.0f, yAxis);

// Transform the point
TriVector rotated = (R * point * ~R).Grade3();
```

!!! warning "Grade Extraction"
    After the sandwich product, call `.Grade3()` to extract the TriVector result.

### Transforming Spheres

Sphere centers are TriVectors:

```cpp
void MyScene::onUpdate(float deltaTime) {
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(deltaTime * 30.0f, yAxis);  // 30 deg/s

    if (auto* sphere = getSphere(m_sphereId)) {
        TriVector center = sphere->getCenter();
        TriVector newCenter = (R * center * ~R).Grade3();
        sphere->setCenter(newCenter);
    }
}
```

### Transforming Mesh Instances

Mesh instances store their transform as a Motor:

```cpp
void MyScene::onUpdate(float deltaTime) {
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(deltaTime * 45.0f, yAxis);

    if (auto* instance = findInstance("myMesh")) {
        // Compose rotation with existing transform
        instance->transform = R * instance->transform;
    }
}
```

## Common Operations

### Position a Mesh

```cpp
// Just translation
Motor position = Motor::Translation(5.0f, 0.0f, -3.0f);
addMeshInstance(meshId, position);

// Or use TriVector directly
addMeshInstance(meshId, TriVector(5.0f, 0.0f, -3.0f));
```

### Rotate and Position

```cpp
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
Motor R = Motor::Rotation(45.0f, yAxis);
Motor T = Motor::Translation(5.0f, 0.0f, 0.0f);

// Rotate first, then move
Motor transform = T * R;
addMeshInstance(meshId, transform);
```

### Orbit Animation

```cpp
void MyScene::onUpdate(float deltaTime) {
    m_angle += deltaTime;  // radians per second

    float radius = 10.0f;
    float x = std::cos(m_angle) * radius;
    float z = std::sin(m_angle) * radius;

    // Position on circle
    TriVector pos(x, 5.0f, z);

    // Also face the center (optional)
    float facingAngle = -m_angle * 57.3f;  // Convert to degrees
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(facingAngle, yAxis);
    Motor T = Motor::Translation(x, 5.0f, z);

    setInstanceTransform(m_instanceId, T * R);
}
```

### Incremental Rotation

For smooth continuous rotation, apply small rotations each frame:

```cpp
void MyScene::onUpdate(float deltaTime) {
    float degreesPerSecond = 45.0f;
    float angleDeg = degreesPerSecond * deltaTime;

    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(angleDeg, yAxis);

    if (auto* instance = findInstance("spinner")) {
        instance->transform = R * instance->transform;
    }
}
```

## Quick Reference

| Type | Represents | Create | Common Use |
|------|------------|--------|------------|
| `TriVector` | Point | `TriVector(x, y, z)` | Positions, sphere centers |
| `Vector` | Plane | `Vector(d, nx, ny, nz)` | Floor, walls, clipping planes |
| `BiVector` | Line/Axis | `BiVector(...)` | Rotation axes |
| `Motor` | Transform | `Motor::Translation()`, `Motor::Rotation()` | Moving/rotating objects |

## Further Reading

- [FlyFish Library](https://github.com/fdlombae/FlyFish) - The PGA library used by FlyTracer
- [Geometric Algebra Primer](https://bivector.net/PGA4CS.html) - PGA for Computer Scientists
- [ganja.js](https://enkimute.github.io/ganja.js/) - Interactive PGA visualizations
