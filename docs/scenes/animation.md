# Animation

FlyTracer uses the **Motor** type from PGA (Projective Geometric Algebra) for object transformations. Motors elegantly combine rotation and translation into a single operation.

## The Update Loop

Animation happens in `OnUpdate`, called every frame with delta time:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;  // Track total elapsed time

    // Animate objects here...
}
```

## Motors

A **Motor** represents a rigid body transformation (rotation + translation). Motors can be composed by multiplication.

### Creating Motors

```cpp
// Identity (no transformation)
Motor M;  // Default constructor

// Pure translation
Motor T = Motor::Translation(x, y, z);

// Pure rotation (angle in degrees, axis as BiVector)
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);  // Y axis
Motor R = Motor::Rotation(45.0f, yAxis);

// Combined: first rotate, then translate
Motor combined = T * R;  // Apply R first, then T
```

### Common Rotation Axes

```cpp
// Y axis (vertical, most common for orbits)
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

// X axis (pitch)
BiVector xAxis(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);

// Z axis (roll)
BiVector zAxis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
```

## Animating Spheres

Spheres store their center as a TriVector. Use the sandwich product to transform:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Incremental rotation each frame
    float angleDegrees = m_PHEASANT_ROTATION_SPEED * deltaTime * 57.3f;  // rad/s to deg
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(angleDegrees, yAxis);

    if (auto* sphere = GetSphere(m_sphereId)) {
        // Transform center: R * center * ~R (sandwich product)
        TriVector oldCenter = sphere->GetCenter();
        TriVector newCenter = (R * oldCenter * ~R).Grade3();
        sphere->SetCenter(newCenter);
    }
}
```

## Animating Meshes

Mesh instances have a `transform` Motor that can be modified directly:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    float angleDeg = 30.0f * deltaTime;  // 30 degrees per second
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(angleDeg, yAxis);

    if (auto* instance = FindInstance("myMesh")) {
        // Compose with existing transform
        instance->transform = R * instance->transform;
    }
}
```

### Translation Animation

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Sine wave movement
    float x = std::sin(m_time) * 5.0f;
    Motor T = Motor::Translation(x, 0.0f, 0.0f);

    if (auto* instance = FindInstance("bouncer")) {
        instance->transform = T;
    }
}
```

### Combined Rotation + Translation

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Rotation
    BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Motor R = Motor::Rotation(m_time * 45.0f, yAxis);  // 45 deg/s

    // Bobbing translation
    float y = std::sin(m_time * 2.0f) * 0.5f;
    Motor T = Motor::Translation(0.0f, y, 0.0f);

    if (auto* instance = FindInstance("floating")) {
        // Rotate first, then bob up/down
        instance->transform = T * R;
    }
}
```