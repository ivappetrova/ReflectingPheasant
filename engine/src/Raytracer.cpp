#include "FlyTracer.h"
#include "Application.h"
#include "Mesh.h"
#include "GameScene.h"

namespace FlyTracer {

// Internal minimal GameScene for the standalone Raytracer API
class RaytracerScene : public GameScene {
public:
    RaytracerScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

    void OnInit(VulkanRenderer* renderer) override {}
    void OnUpdate(float deltaTime) override {}
    void OnInput(const InputState& input) override {}
    void OnGui() override {}
    void OnShutdown() override {}
};

struct Raytracer::Impl {
    Config config;
    std::unique_ptr<Application> application;
    std::unique_ptr<Mesh> mesh;
    Camera camera;
    std::vector<Light> lights;
    std::vector<Sphere> spheres;
    std::vector<Plane> planes;
    UpdateCallback updateCallback;
    KeyCallback keyCallback;
    MouseCallback mouseCallback;
    float fps{0.0f};
    float frameTime{0.0f};
    int frameCount{0};
    bool running{false};
};

Raytracer::Raytracer(const Config& config)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
    m_impl->mesh = std::make_unique<Mesh>();

    // Set default camera
    m_impl->camera.position[0] = 0.0f;
    m_impl->camera.position[1] = 2.0f;
    m_impl->camera.position[2] = 5.0f;
    m_impl->camera.target[0] = 0.0f;
    m_impl->camera.target[1] = 0.0f;
    m_impl->camera.target[2] = 0.0f;
    m_impl->camera.up[0] = 0.0f;
    m_impl->camera.up[1] = 1.0f;
    m_impl->camera.up[2] = 0.0f;
    m_impl->camera.fov = 60.0f;
}

Raytracer::~Raytracer() = default;

Raytracer::Raytracer(Raytracer&&) noexcept = default;
Raytracer& Raytracer::operator=(Raytracer&&) noexcept = default;

bool Raytracer::loadMesh(const std::string& objPath) {
    if (!m_impl->mesh) {
        m_impl->mesh = std::make_unique<Mesh>();
    }
    return m_impl->mesh->LoadFromFile(objPath);
}

bool Raytracer::loadMesh(const std::string& objPath, const std::string& mtlBasePath) {
    if (!m_impl->mesh) {
        m_impl->mesh = std::make_unique<Mesh>();
    }
    return m_impl->mesh->LoadFromFile(objPath, mtlBasePath);
}

void Raytracer::setCamera(const Camera& camera) {
    m_impl->camera = camera;
}

void Raytracer::addLight(const Light& light) {
    m_impl->lights.push_back(light);
}

void Raytracer::addSphere(const Sphere& sphere) {
    m_impl->spheres.push_back(sphere);
}

void Raytracer::addPlane(const Plane& plane) {
    m_impl->planes.push_back(plane);
}

void Raytracer::clearScene() {
    m_impl->mesh = std::make_unique<Mesh>();
    m_impl->lights.clear();
    m_impl->spheres.clear();
    m_impl->planes.clear();
}

void Raytracer::setUpdateCallback(UpdateCallback callback) {
    m_impl->updateCallback = std::move(callback);
}

void Raytracer::setKeyCallback(KeyCallback callback) {
    m_impl->keyCallback = std::move(callback);
}

void Raytracer::setMouseCallback(MouseCallback callback) {
    m_impl->mouseCallback = std::move(callback);
}

void Raytracer::run() {
    m_impl->running = true;

    // Create a minimal scene for the standalone Raytracer API
    auto scene = std::make_unique<RaytracerScene>(m_impl->config.resourcePath);

    m_impl->application = std::make_unique<Application>(
        m_impl->config.width,
        m_impl->config.height,
        m_impl->config.title,
        std::move(scene),
        m_impl->config.shaderPath
    );
    m_impl->application->run();
    m_impl->running = false;
}

bool Raytracer::isRunning() const {
    return m_impl->running;
}

void Raytracer::stop() {
    m_impl->running = false;
}

float Raytracer::getFPS() const {
    return m_impl->fps;
}

float Raytracer::getFrameTime() const {
    return m_impl->frameTime;
}

int Raytracer::getFrameCount() const {
    return m_impl->frameCount;
}

const Config& Raytracer::getConfig() const {
    return m_impl->config;
}

int Raytracer::getWidth() const {
    return m_impl->config.width;
}

int Raytracer::getHeight() const {
    return m_impl->config.height;
}

} // namespace FlyTracer
