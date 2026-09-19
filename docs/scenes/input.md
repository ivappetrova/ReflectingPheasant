# Input Handling

The `onInput` method receives an `InputState` struct every frame with the current state of mouse and keyboard.

## InputState Structure

```cpp
struct InputState {
    // Mouse buttons
    bool leftMouseDown{false};
    bool rightMouseDown{false};
    bool middleMouseDown{false};

    // Mouse position and movement
    float mouseX{0.0f};          // Current X position
    float mouseY{0.0f};          // Current Y position
    float mouseDeltaX{0.0f};     // Movement since last frame
    float mouseDeltaY{0.0f};     // Movement since last frame
    float scrollDelta{0.0f};     // Scroll wheel delta

    // Keyboard
    bool keyW{false}, keyA{false}, keyS{false}, keyD{false};
    bool keyQ{false}, keyE{false};
    bool keyUp{false}, keyDown{false}, keyLeft{false}, keyRight{false};

    // Timing
    float deltaTime{0.0f};       // Time since last frame
};
```

## Basic Input Handling

```cpp
void MyScene::OnInput(const InputState& input) {
    // Check mouse buttons
    if (input.leftMouseDown) {
        // Left mouse is held
    }

    // Check keys
    if (input.keyW) {
        // W is pressed
    }

    // Use delta time for frame-independent movement
    float moveSpeed = 10.0f * input.deltaTime;
}
```
