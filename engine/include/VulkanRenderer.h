#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <string>
#include <memory>

class Mesh;
struct MeshInstance;
namespace Scene { struct SceneData; struct GPUSphere; struct GPUPlane; }

// Handles all Vulkan resources and rendering
class VulkanRenderer {
public:
    struct Config {
        int width{1280};
        int height{720};
        bool enableValidation{true};
        std::string shaderDir{"shaders"};
    };

    VulkanRenderer(SDL_Window* window, const Config& config);
    ~VulkanRenderer() noexcept;

    // Non-copyable, non-movable
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Rendering
    void BeginFrame();
    void RenderScene(const Scene::SceneData& sceneData,
                     const std::vector<std::unique_ptr<Mesh>>& meshes,
                     const std::vector<MeshInstance>& instances,
                     const float cameraRotation[9],  // 3x3 rotation matrix (row-major)
                     const float cameraPosition[3],
                     float time, float cameraFov);
    void EndFrame();

    // Resource management - Multi-mesh support
    void UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes);
    void UploadSceneData(const Scene::SceneData& sceneData);
    void UploadInstances(const std::vector<MeshInstance>& instances);  // Upload mesh instance transforms
    void UpdateSpheres(const std::vector<Scene::GPUSphere>& spheres);  // Update sphere buffer for animation
    void UpdatePlanes(const std::vector<Scene::GPUPlane>& planes);    // Update plane buffer for animation
    void UploadTexture(const std::string& filename);
    void WaitIdle();

    // Post-upload finalization - call after all uploads are complete
    void CreateDescriptorSet();  // Create and bind descriptor set after uploads
    void CreateComputePipeline();  // Create compute pipeline after descriptor set

    // Memory optimization
    bool AreMeshesUploaded() const { return m_meshesUploaded; }

    // Getters
    VkDevice Device() const { return m_device; }
    VkPhysicalDevice PhysicalDevice() const { return m_physicalDevice; }
    VkCommandPool CommandPool() const { return m_commandPool; }
    VkQueue ComputeQueue() const { return m_computeQueue; }

private:
    // Push constants structure for compute shader
    // Must match shader layout exactly
    struct PushConstants {
        float time;
        uint32_t triangleCount;
        uint32_t sphereCount;
        uint32_t planeCount;
        uint32_t lightCount;
        uint32_t materialCount;
        uint32_t instanceCount;
        float planeTileScale;
        // Camera basis vectors (vec4 with w unused for padding)
        std::array<float, 4> cameraRight;    // X axis in world space
        std::array<float, 4> cameraUp;       // Y axis in world space
        std::array<float, 4> cameraForward;  // -Z axis in world space
        std::array<float, 3> cameraPosition;
        float cameraFov;
        uint32_t textureWidth;
        uint32_t textureHeight;
        uint32_t maxBounces;
    } m_pushConstants{};

    // Initialization
    void createInstance();
    void setupDebugMessenger();
    void selectPhysicalDevice();
    void createDevice();
    void createSwapchain();
    void createRenderResources();
    void createSyncObjects();
    void recreateSwapchain();

    // Resource creation helpers
    void createCommandPool();
    void createStorageImage();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createSwapchainImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createImGuiResources();

    // Cleanup
    void cleanup();
    void cleanupSwapchain();

    // Helper functions
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
    [[nodiscard]] std::vector<char> readFile(const std::string& filename) const;
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;

    // Window and config
    SDL_Window* m_window;
    Config m_config;

    // Vulkan core
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    // Queues
    VkQueue m_computeQueue{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    uint32_t m_computeQueueFamily{0};
    uint32_t m_graphicsQueueFamily{0};
    uint32_t m_presentQueueFamily{0};

    // Swapchain
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainImageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_swapchainExtent{};

    // Compute pipeline
    VkImage m_storageImage{VK_NULL_HANDLE};
    VkDeviceMemory m_storageImageMemory{VK_NULL_HANDLE};
    VkImageView m_storageImageView{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_computePipeline{VK_NULL_HANDLE};

    // Scene buffers
    VkBuffer m_vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_vertexBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_indexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_indexBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_bvhNodeBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_bvhNodeBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_bvhTriIdxBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_bvhTriIdxBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_meshInfoBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_meshInfoBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_sphereBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_sphereBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_planeBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_planeBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_lightBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_lightBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_materialBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_materialBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_textureBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_textureBufferMemory{VK_NULL_HANDLE};
    VkBuffer m_textureInfoBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_textureInfoBufferMemory{VK_NULL_HANDLE};
    uint32_t m_textureWidth{0};
    uint32_t m_textureHeight{0};
    VkBuffer m_instanceMotorBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_instanceMotorBufferMemory{VK_NULL_HANDLE};
    uint32_t m_instanceBufferCapacity{0};  // Track allocated capacity for dynamic resize

    // Storage image layout tracking
    VkImageLayout m_storageImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

    // Commands and sync
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_computeFinishedSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;  // Track which fence is using each swapchain image
    uint32_t m_currentFrame{0};

    // ImGui rendering resources
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_framebuffers;
    VkDescriptorPool m_imguiDescriptorPool{VK_NULL_HANDLE};
    VkCommandPool m_imguiCommandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> m_imguiCommandBuffers;

    // Frame state
    uint32_t m_imageIndex{0};
    bool m_frameStarted{false};
    bool m_meshesUploaded{false};

    // Modern Vulkan 1.3 extension function pointers (loaded dynamically for MoltenVK compatibility)
    PFN_vkQueueSubmit2KHR m_vkQueueSubmit2KHR{nullptr};
    PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR{nullptr};
    PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR{nullptr};
    PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR{nullptr};
};
