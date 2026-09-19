# Camera

FlyTracer uses a simple **eye-target-up** camera model with PGA TriVectors for positions.

## Camera Properties

| Property | Type | Description                                              |
|----------|------|----------------------------------------------------------|
| `m_cameraEye` | `TriVector` | Camera position in world space                           |
| `m_cameraTarget` | `TriVector` | Point the camera looks at                                |
| `m_cameraUp` | `TriVector` | Up direction (usually Y-axis), ideal point with e123 = 0 |
| `m_cameraFov` | `float` | Field of view in degrees                                 |

## Setting the Camera

### In OnInit

```cpp
void MyScene::OnInit(VulkanRenderer* renderer) {
    // Position camera
    m_cameraEye = TriVector(0.0f, 5.0f, 15.0f);     // Behind and above
    m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);   // Look at center
    m_cameraUp = TriVector(0.0f, 1.0f, 0.0f);       // Y is up

    // Optional: adjust FOV (default is 45)
    m_cameraFov = 60.0f;
}
```

### Using SetCamera

```cpp
SetCamera(
    TriVector(eyeX, eyeY, eyeZ),
    TriVector(targetX, targetY, targetZ),
    TriVector(upX, upY, upZ)
);

SetCameraFov(75.0f);
```
