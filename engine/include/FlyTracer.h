#pragma once

#include <string>
#include <memory>
#include <functional>

// Forward declarations
class Application;
class Mesh;

namespace FlyTracer {

// Configuration for the raytracer
struct Config {
    int width{1280};
    int height{720};
    std::string title{"FlyTracer"};
    std::string resourcePath{"resources"};
    std::string shaderPath{"shaders"};
    int maxRayBounces{4};
    bool enableImGui{true};
    bool vsync{true};
};

// Light types for scene lighting
enum class LightType {
    Directional,
    Point,
    Spot
};

// Light definition for external scene setup
struct Light {
    LightType type{LightType::Directional};
    float position[3]{0.0f, 0.0f, 0.0f};
    float direction[3]{0.0f, -1.0f, 0.0f};
    float color[3]{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{10.0f};         // For point/spot lights
    float spotAngle{0.5f};      // For spot lights (radians)
    float spotSoftness{0.2f};   // For spot lights
};

// Sphere primitive for scene building
struct Sphere {
    float center[3]{0.0f, 0.0f, 0.0f};
    float radius{1.0f};
    float color[3]{1.0f, 1.0f, 1.0f};
    float reflectivity{0.0f};
};

// Plane primitive for scene building
struct Plane {
    float point[3]{0.0f, 0.0f, 0.0f};
    float normal[3]{0.0f, 1.0f, 0.0f};
    float color[3]{0.5f, 0.5f, 0.5f};
    float reflectivity{0.1f};
};

// Camera configuration
struct Camera {
    float position[3]{0.0f, 2.0f, 5.0f};
    float target[3]{0.0f, 0.0f, 0.0f};
    float up[3]{0.0f, 1.0f, 0.0f};
    float fov{60.0f};           // Field of view in degrees
    float nearPlane{0.1f};
    float farPlane{100.0f};
};

// Callback types for user interaction
using UpdateCallback = std::function<void(float deltaTime)>;
using KeyCallback = std::function<void(int key, bool pressed)>;
using MouseCallback = std::function<void(int x, int y, int button, bool pressed)>;

// Main raytracer class
class Raytracer {
public:
    explicit Raytracer(const Config& config = Config{});
    ~Raytracer();

    // Non-copyable
    Raytracer(const Raytracer&) = delete;
    Raytracer& operator=(const Raytracer&) = delete;

    // Movable
    Raytracer(Raytracer&&) noexcept;
    Raytracer& operator=(Raytracer&&) noexcept;

    // Scene setup
    bool loadMesh(const std::string& objPath);
    bool loadMesh(const std::string& objPath, const std::string& mtlBasePath);
    void setCamera(const Camera& camera);
    void addLight(const Light& light);
    void addSphere(const Sphere& sphere);
    void addPlane(const Plane& plane);
    void clearScene();

    // Callbacks
    void setUpdateCallback(UpdateCallback callback);
    void setKeyCallback(KeyCallback callback);
    void setMouseCallback(MouseCallback callback);

    // Running
    void run();
    bool isRunning() const;
    void stop();

    // Runtime state
    float getFPS() const;
    float getFrameTime() const;
    int getFrameCount() const;

    // Configuration getters
    const Config& getConfig() const;
    int getWidth() const;
    int getHeight() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Version information
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

inline std::string getVersionString() {
    return std::to_string(VERSION_MAJOR) + "." +
           std::to_string(VERSION_MINOR) + "." +
           std::to_string(VERSION_PATCH);
}

} // namespace FlyTracer

// Convenience re-exports for mesh manipulation
#include "Config.h"
