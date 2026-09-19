#define NOMINMAX
#include "VulkanRenderer.h"
#include "Mesh.h"
#include "Scene.h"
#include "GameScene.h"
#include "VulkanHelpers.h"
#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include "stb_image.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <set>
#include <unordered_map>

// ============================================================================
// Constants and Helper Functions (moved from Application.cpp)
// ============================================================================

// Validation layers
// VK_LAYER_KHRONOS_validation: Core validation (API usage, object lifetime, threading)
static const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Optional debug layers (can be enabled via environment or code)
// VK_LAYER_LUNARG_api_dump: Logs all Vulkan API calls (very verbose)
static const std::vector<const char*> optionalDebugLayers = {
    "VK_LAYER_LUNARG_api_dump"
};

// Enable validation layers in debug builds
#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

// Environment variable to enable API dump layer
static bool isApiDumpEnabled() {
    const char* env = std::getenv("FLYTRACER_API_DUMP");
    return env != nullptr && std::string(env) == "1";
}

// Debug callback function for validation layer messages
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)pUserData;  // Unused

    // Skip verbose messages unless they're from the loader (useful for debugging layer loading)
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        // Only show loader messages in verbose mode
        if (pCallbackData->pMessageIdName == nullptr ||
            strstr(pCallbackData->pMessageIdName, "Loader") == nullptr) {
            return VK_FALSE;
        }
    }

    // Determine severity prefix and color
    const char* severityStr = "";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severityStr = "\033[31m[ERROR]\033[0m";  // Red
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severityStr = "\033[33m[WARNING]\033[0m";  // Yellow
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severityStr = "\033[36m[INFO]\033[0m";  // Cyan
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severityStr = "\033[90m[VERBOSE]\033[0m";  // Gray
    }

    // Determine message type(s) - can have multiple
    std::string typeStr;
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        typeStr += "GENERAL";
    }
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        if (!typeStr.empty()) typeStr += "|";
        typeStr += "VALIDATION";
    }
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        if (!typeStr.empty()) typeStr += "|";
        typeStr += "PERFORMANCE";
    }

    // Print main message
    std::cerr << "Vulkan " << severityStr << " " << typeStr << ": ";

    // Include message ID if available (helps identify specific validation checks)
    if (pCallbackData->pMessageIdName != nullptr) {
        std::cerr << "[" << pCallbackData->pMessageIdName << "] ";
    }

    std::cerr << pCallbackData->pMessage << std::endl;

    // Print related objects (helps identify which Vulkan objects are involved)
    if (pCallbackData->objectCount > 0) {
        std::cerr << "  Related objects:" << std::endl;
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
            const auto& object = pCallbackData->pObjects[i];
            std::cerr << "    [" << i << "] Type: " << object.objectType
                      << ", Handle: 0x" << std::hex << object.objectHandle << std::dec;
            if (object.pObjectName != nullptr) {
                std::cerr << ", Name: \"" << object.pObjectName << "\"";
            }
            std::cerr << std::endl;
        }
    }

    // Print queue labels (if any command buffer labels are active)
    if (pCallbackData->queueLabelCount > 0) {
        std::cerr << "  Queue labels:" << std::endl;
        for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++) {
            std::cerr << "    " << pCallbackData->pQueueLabels[i].pLabelName << std::endl;
        }
    }

    // Print command buffer labels
    if (pCallbackData->cmdBufLabelCount > 0) {
        std::cerr << "  Command buffer labels:" << std::endl;
        for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
            std::cerr << "    " << pCallbackData->pCmdBufLabels[i].pLabelName << std::endl;
        }
    }

    // Return VK_FALSE to indicate the call should not be aborted
    return VK_FALSE;
}

// Helper function to populate debug messenger create info
static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    // Enable all severity levels - our callback filters appropriately
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Enable standard message types
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

// Helper to check if a specific layer is available
static bool isLayerAvailable(const char* layerName, const std::vector<VkLayerProperties>& availableLayers) {
    for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
            return true;
        }
    }
    return false;
}

// Get the list of layers to enable (filters out unavailable layers with warnings)
static std::vector<const char*> getEnabledLayers() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char*> enabledLayers;

    // Add required validation layers
    for (const char* layerName : validationLayers) {
        if (isLayerAvailable(layerName, availableLayers)) {
            enabledLayers.push_back(layerName);
            std::cout << "  Enabling layer: " << layerName << std::endl;
        } else {
            std::cerr << "  Warning: Layer not available: " << layerName << std::endl;
        }
    }

    // Add optional debug layers if requested
    if (isApiDumpEnabled()) {
        for (const char* layerName : optionalDebugLayers) {
            if (isLayerAvailable(layerName, availableLayers)) {
                enabledLayers.push_back(layerName);
                std::cout << "  Enabling optional layer: " << layerName << std::endl;
            }
        }
    }

    return enabledLayers;
}

// Check if validation layers are supported
static bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Check if at least the core validation layer is available
    bool coreValidationAvailable = false;
    for (const auto& layerProperties : availableLayers) {
        if (strcmp("VK_LAYER_KHRONOS_validation", layerProperties.layerName) == 0) {
            coreValidationAvailable = true;
            break;
        }
    }

    if (!coreValidationAvailable) {
        std::cerr << "Core validation layer (VK_LAYER_KHRONOS_validation) not found!\n";
        std::cerr << "Available layers:\n";
        for (const auto& layer : availableLayers) {
            std::cerr << "  " << layer.layerName << " (v" << layer.specVersion << ")\n";
        }
    }

    return coreValidationAvailable;
}

// ============================================================================
// VulkanRenderer Implementation
// ============================================================================

VulkanRenderer::VulkanRenderer(SDL_Window* window, const Config& config)
    : m_window(window), m_config(config)
{
    // Initialize Vulkan in the correct order
    createInstance();

    // Create surface using SDL (requires instance and window)
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    }

    setupDebugMessenger();
    selectPhysicalDevice();
    createDevice();
    createSwapchain();
    createRenderResources();
}

VulkanRenderer::~VulkanRenderer() noexcept {
    cleanup();
}

void VulkanRenderer::BeginFrame() {
    // Wait for the previous frame using this slot to complete
    // Use a reasonable timeout (5 seconds) to detect GPU hangs instead of infinite wait
    constexpr uint64_t kFenceTimeoutNs = 5'000'000'000ULL;  // 5 seconds
    VkResult fenceResult = vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, kFenceTimeoutNs);

    if (fenceResult == VK_TIMEOUT) {
        std::cerr << "Warning: GPU fence timeout - possible GPU hang or extremely long frame\n";
        // Try to recover by waiting longer
        fenceResult = vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, kFenceTimeoutNs * 2);
        if (fenceResult != VK_SUCCESS) {
            throw std::runtime_error("GPU fence timeout - unable to recover");
        }
    } else if (fenceResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fence: " + std::to_string(fenceResult));
    }

    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, kFenceTimeoutNs,
                                           m_imageAvailableSemaphores[m_currentFrame],
                                           VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (result == VK_TIMEOUT) {
        std::cerr << "Warning: Swapchain image acquisition timeout\n";
        return;  // Skip this frame
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
    }

    // Check if a previous frame is still using this swapchain image
    // This can happen when m_currentFrame differs from the swapchain's returned image index
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE) {
        fenceResult = vkWaitForFences(m_device, 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, kFenceTimeoutNs);
        if (fenceResult != VK_SUCCESS && fenceResult != VK_TIMEOUT) {
            throw std::runtime_error("Failed to wait for image-in-flight fence");
        }
    }
    // Mark this image as now being used by this frame's fence
    m_imagesInFlight[m_imageIndex] = m_inFlightFences[m_currentFrame];

    // Only reset fence after we know we'll submit work
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    // Start ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    m_frameStarted = true;
}

void VulkanRenderer::RenderScene(const Scene::SceneData& sceneData,
                                  const std::vector<std::unique_ptr<Mesh>>& meshes,
                                  const std::vector<MeshInstance>& instances,
                                  const float cameraRotation[9],
                                  const float cameraPosition[3],
                                  float time, float cameraFov) {
    if (!m_frameStarted) {
        return;
    }

    // Update push constants
    m_pushConstants.time = time;
    m_pushConstants.triangleCount = meshes.empty() || !meshes[0] ? 0 : static_cast<uint32_t>(meshes[0]->TriangleCount());
    m_pushConstants.sphereCount = static_cast<uint32_t>(sceneData.spheres.size());
    m_pushConstants.planeCount = static_cast<uint32_t>(sceneData.planes.size());
    m_pushConstants.lightCount = static_cast<uint32_t>(sceneData.lights.size());
    m_pushConstants.materialCount = sceneData.materials.empty() ? 1 : static_cast<uint32_t>(sceneData.materials.size());
    m_pushConstants.instanceCount = static_cast<uint32_t>(instances.size());
    m_pushConstants.planeTileScale = 0.1f;  // Tile scale for plane UV mapping

    // Camera rotation matrix is row-major: row 0 = right, row 1 = up, row 2 = forward
    // Each row becomes a vec4 (with w = 0 for padding)
    m_pushConstants.cameraRight[0] = cameraRotation[0];
    m_pushConstants.cameraRight[1] = cameraRotation[1];
    m_pushConstants.cameraRight[2] = cameraRotation[2];
    m_pushConstants.cameraRight[3] = 0.0f;

    m_pushConstants.cameraUp[0] = cameraRotation[3];
    m_pushConstants.cameraUp[1] = cameraRotation[4];
    m_pushConstants.cameraUp[2] = cameraRotation[5];
    m_pushConstants.cameraUp[3] = 0.0f;

    m_pushConstants.cameraForward[0] = cameraRotation[6];
    m_pushConstants.cameraForward[1] = cameraRotation[7];
    m_pushConstants.cameraForward[2] = cameraRotation[8];
    m_pushConstants.cameraForward[3] = 0.0f;

    m_pushConstants.cameraPosition[0] = cameraPosition[0];
    m_pushConstants.cameraPosition[1] = cameraPosition[1];
    m_pushConstants.cameraPosition[2] = cameraPosition[2];
    m_pushConstants.cameraFov = cameraFov;
    m_pushConstants.textureWidth = m_textureWidth;
    m_pushConstants.textureHeight = m_textureHeight;
    m_pushConstants.maxBounces = 3;  // Default: 3 bounces for reflections/shadows

    // Record compute commands
    VkCommandBuffer cmdBuffer = m_commandBuffers[m_currentFrame];
    VkResult resetResult = vkResetCommandBuffer(cmdBuffer, 0);
    if (resetResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to reset command buffer: " << resetResult << "\n";
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult beginResult = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to begin command buffer: " << beginResult << "\n";
        return;
    }

    // Bind compute pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    // Push constants
    vkCmdPushConstants(cmdBuffer, m_pipelineLayout,
                      VK_SHADER_STAGE_COMPUTE_BIT, 0,
                      sizeof(PushConstants), &m_pushConstants);

    // Dispatch compute shader
    vkCmdDispatch(cmdBuffer,
                 (m_config.width + 15) / 16,
                 (m_config.height + 15) / 16,
                 1);

    // Modern Vulkan 1.3: Use synchronization2 barriers with 64-bit flags
    // Transition storage image for transfer
    VulkanHelpers::transitionImageLayout2(cmdBuffer, m_storageImage,
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_SHADER_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_COPY_BIT,
                         VK_ACCESS_2_TRANSFER_READ_BIT,
                         m_vkCmdPipelineBarrier2KHR);

    // Transition swapchain image for transfer (use NONE instead of TOP_OF_PIPE)
    VulkanHelpers::transitionImageLayout2(cmdBuffer, m_swapchainImages[m_imageIndex],
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_PIPELINE_STAGE_2_NONE,
                         VK_ACCESS_2_NONE,
                         VK_PIPELINE_STAGE_2_COPY_BIT,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         m_vkCmdPipelineBarrier2KHR);

    // Copy storage image to swapchain
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width = std::min(static_cast<uint32_t>(m_config.width), m_swapchainExtent.width);
    copyRegion.extent.height = std::min(static_cast<uint32_t>(m_config.height), m_swapchainExtent.height);
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(cmdBuffer,
                  m_storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  m_swapchainImages[m_imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &copyRegion);

    // Transition storage image back to GENERAL
    VulkanHelpers::transitionImageLayout2(cmdBuffer, m_storageImage,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_PIPELINE_STAGE_2_COPY_BIT,
                         VK_ACCESS_2_TRANSFER_READ_BIT,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_SHADER_WRITE_BIT,
                         m_vkCmdPipelineBarrier2KHR);

    VkResult endResult = vkEndCommandBuffer(cmdBuffer);
    if (endResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to end command buffer in renderScene: " << endResult << "\n";
        return;
    }

    // Modern Vulkan 1.3: Use vkQueueSubmit2 with VkSubmitInfo2
    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = m_imageAvailableSemaphores[m_currentFrame];
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COPY_BIT;  // More precise than TRANSFER_BIT
    waitSemaphoreInfo.value = 0;  // Binary semaphore

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = m_computeFinishedSemaphores[m_currentFrame];
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreInfo.value = 0;

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{};
    cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferSubmitInfo.commandBuffer = cmdBuffer;

    VkSubmitInfo2 submitInfo2{};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = 1;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &cmdBufferSubmitInfo;
    submitInfo2.signalSemaphoreInfoCount = 1;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    if (m_vkQueueSubmit2KHR(m_computeQueue, 1, &submitInfo2, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer");
    }
}

void VulkanRenderer::EndFrame() {
    if (!m_frameStarted) {
        return;
    }

    // Render ImGui (Application should have built UI between beginFrame and endFrame)
    ImGui::Render();

    // Record ImGui commands
    // Use m_currentFrame for consistent synchronization (not m_imageIndex which comes from swapchain)
    VkCommandBuffer imguiCmd = m_imguiCommandBuffers[m_currentFrame];
    VkResult resetResult = vkResetCommandBuffer(imguiCmd, 0);
    if (resetResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to reset ImGui command buffer: " << resetResult << "\n";
        m_frameStarted = false;
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult beginResult = vkBeginCommandBuffer(imguiCmd, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to begin ImGui command buffer: " << beginResult << "\n";
        m_frameStarted = false;
        return;
    }

    // Use traditional render pass for ImGui (workaround for MoltenVK flickering with dynamic rendering)
    // The render pass handles layout transitions:
    //   initialLayout: TRANSFER_DST_OPTIMAL (from compute blit)
    //   finalLayout: PRESENT_SRC_KHR (ready for presentation)
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[m_imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    // No clear values needed since loadOp is LOAD (preserves raytraced image)

    vkCmdBeginRenderPass(imguiCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguiCmd);
    vkCmdEndRenderPass(imguiCmd);

    VkResult endResult = vkEndCommandBuffer(imguiCmd);
    if (endResult != VK_SUCCESS) {
        std::cerr << "Warning: Failed to end ImGui command buffer: " << endResult << "\n";
        m_frameStarted = false;
        return;
    }

    // Modern Vulkan 1.3: Use vkQueueSubmit2
    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = m_computeFinishedSemaphores[m_currentFrame];
    // Wait at COPY stage to ensure layout transition (which has srcStage=COPY) waits for
    // the compute submission's copy operation to complete before starting
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    waitSemaphoreInfo.value = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = m_renderFinishedSemaphores[m_currentFrame];
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreInfo.value = 0;

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{};
    cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferSubmitInfo.commandBuffer = imguiCmd;

    VkSubmitInfo2 submitInfo2{};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = 1;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &cmdBufferSubmitInfo;
    submitInfo2.signalSemaphoreInfoCount = 1;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    if (m_vkQueueSubmit2KHR(m_graphicsQueue, 1, &submitInfo2, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit ImGui command buffer");
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    VkSemaphore presentWaitSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    presentInfo.pWaitSemaphores = presentWaitSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_currentFrame = (m_currentFrame + 1) % m_swapchainImages.size();
    m_frameStarted = false;
}

void VulkanRenderer::UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes) {
    // Clean up existing buffers before creating new ones to avoid memory leaks
    auto cleanupExistingBuffers = [this]() {
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
        if (m_indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_bvhNodeBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_bvhNodeBuffer, nullptr);
            m_bvhNodeBuffer = VK_NULL_HANDLE;
        }
        if (m_bvhNodeBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_bvhNodeBufferMemory, nullptr);
            m_bvhNodeBufferMemory = VK_NULL_HANDLE;
        }
        if (m_bvhTriIdxBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_bvhTriIdxBuffer, nullptr);
            m_bvhTriIdxBuffer = VK_NULL_HANDLE;
        }
        if (m_bvhTriIdxBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_bvhTriIdxBufferMemory, nullptr);
            m_bvhTriIdxBufferMemory = VK_NULL_HANDLE;
        }
        if (m_meshInfoBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_meshInfoBuffer, nullptr);
            m_meshInfoBuffer = VK_NULL_HANDLE;
        }
        if (m_meshInfoBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_meshInfoBufferMemory, nullptr);
            m_meshInfoBufferMemory = VK_NULL_HANDLE;
        }
    };

    cleanupExistingBuffers();

    if (meshes.empty() || !meshes[0]) {
        std::cerr << "Warning: No meshes to upload\n";
        // Create minimal dummy buffers so descriptor set binding doesn't crash
        VkDeviceSize dummySize = sizeof(float);  // Minimal size
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_vertexBuffer, m_vertexBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_indexBuffer, m_indexBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_materialBuffer, m_materialBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_bvhNodeBuffer, m_bvhNodeBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_bvhTriIdxBuffer, m_bvhTriIdxBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_meshInfoBuffer, m_meshInfoBufferMemory);
        VulkanHelpers::createBuffer(m_device, m_physicalDevice, dummySize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_textureInfoBuffer, m_textureInfoBufferMemory);
        return;
    }

    // Concatenate all mesh data and track per-mesh offsets
    std::vector<GPUVertex> allVertices;
    std::vector<Triangle> allTriangles;
    std::vector<Scene::BVHNode> allBvhNodes;
    std::vector<uint32_t> allBvhTriIndices;
    std::vector<Scene::GPUMeshInfo> meshInfos;
    std::vector<Scene::GPUMaterial> allMaterials;

    // Collect unique texture paths and map to indices
    std::vector<std::string> texturePaths;
    std::unordered_map<std::string, int32_t> texturePathToIndex;

    uint32_t vertexOffset = 0;
    uint32_t triangleOffset = 0;
    uint32_t bvhNodeOffset = 0;
    uint32_t bvhTriIdxOffset = 0;
    uint32_t materialOffset = 0;

    for (const auto& mesh : meshes) {
        if (!mesh) continue;

        Scene::GPUMeshInfo info{};
        info.vertexOffset = vertexOffset;
        info.triangleOffset = triangleOffset;
        info.bvhNodeOffset = bvhNodeOffset;
        info.bvhTriIdxOffset = bvhTriIdxOffset;

        // Add vertices
        const auto& vertices = mesh->Vertices();
        for (const auto& v : vertices) {
            allVertices.push_back(v.ToGPU());
        }

        // Add materials and track texture indices
        for (size_t i = 0; i < mesh->MaterialCount(); ++i) {
            const auto& mat = mesh->GetMaterial(i);
            Scene::GPUMaterial gpuMat = mat.ToGPU();

            // Handle texture path
            if (!mat.diffuseTexturePath.empty()) {
                auto it = texturePathToIndex.find(mat.diffuseTexturePath);
                if (it == texturePathToIndex.end()) {
                    // New texture - add to list
                    int32_t texIdx = static_cast<int32_t>(texturePaths.size());
                    texturePaths.push_back(mat.diffuseTexturePath);
                    texturePathToIndex[mat.diffuseTexturePath] = texIdx;
                    gpuMat.diffuseTextureIndex = texIdx;
                } else {
                    gpuMat.diffuseTextureIndex = it->second;
                }
            } else {
                gpuMat.diffuseTextureIndex = -1;
            }

            allMaterials.push_back(gpuMat);
        }

        // Add triangles with adjusted vertex AND material indices
        const auto& triangles = mesh->Triangles();
        for (const auto& tri : triangles) {
            Triangle adjustedTri = tri;
            adjustedTri.indices[0] += vertexOffset;
            adjustedTri.indices[1] += vertexOffset;
            adjustedTri.indices[2] += vertexOffset;
            adjustedTri.materialIndex += materialOffset;
            allTriangles.push_back(adjustedTri);
        }
        info.triangleCount = static_cast<uint32_t>(triangles.size());

        // Add BVH nodes with adjusted child/triangle indices
        const auto& bvhNodes = mesh->BVHNodes();
        for (const auto& node : bvhNodes) {
            Scene::BVHNode adjustedNode = node;
            if (node.triCount > 0) {
                // Leaf node: leftFirst is index into bvhTriIndices
                adjustedNode.leftFirst += static_cast<int32_t>(bvhTriIdxOffset);
            } else {
                // Internal node: leftFirst is index of left child node
                adjustedNode.leftFirst += static_cast<int32_t>(bvhNodeOffset);
            }
            allBvhNodes.push_back(adjustedNode);
        }
        info.bvhNodeCount = static_cast<uint32_t>(bvhNodes.size());

        // Add BVH triangle indices with adjusted triangle offsets
        const auto& bvhTriIndices = mesh->BVHTriIndices();
        for (uint32_t idx : bvhTriIndices) {
            allBvhTriIndices.push_back(idx + triangleOffset);
        }

        // Update offsets for next mesh
        vertexOffset += static_cast<uint32_t>(vertices.size());
        triangleOffset += static_cast<uint32_t>(triangles.size());
        bvhNodeOffset += static_cast<uint32_t>(bvhNodes.size());
        bvhTriIdxOffset += static_cast<uint32_t>(bvhTriIndices.size());
        materialOffset += static_cast<uint32_t>(mesh->MaterialCount());

        meshInfos.push_back(info);
    }

    // Ensure we have at least one mesh info entry and one material
    if (meshInfos.empty()) {
        meshInfos.push_back(Scene::GPUMeshInfo{});
    }
    if (allMaterials.empty()) {
        Scene::GPUMaterial defaultMat{};
        defaultMat.diffuse[0] = 0.8f;
        defaultMat.diffuse[1] = 0.8f;
        defaultMat.diffuse[2] = 0.8f;
        defaultMat.shininess = 32.0f;
        defaultMat.shadingMode = static_cast<int32_t>(Scene::ShadingMode::PBR);
        defaultMat.diffuseTextureIndex = -1;
        defaultMat.metalness = 0.0f;
        defaultMat.roughness = 0.5f;
        allMaterials.push_back(defaultMat);
    }

    VkDeviceSize vertexBufferSize = sizeof(GPUVertex) * allVertices.size();
    VkDeviceSize indexBufferSize = sizeof(Triangle) * allTriangles.size();

    // Create staging buffers
    VkBuffer vertexStagingBuffer, indexStagingBuffer;
    VkDeviceMemory vertexStagingMemory, indexStagingMemory;

    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexStagingBuffer, vertexStagingMemory);

    createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indexStagingBuffer, indexStagingMemory);

    // Copy GPU-compatible vertex data to staging buffer
    void* data;
    vkMapMemory(m_device, vertexStagingMemory, 0, vertexBufferSize, 0, &data);
    std::memcpy(data, allVertices.data(), vertexBufferSize);
    vkUnmapMemory(m_device, vertexStagingMemory);

    vkMapMemory(m_device, indexStagingMemory, 0, indexBufferSize, 0, &data);
    std::memcpy(data, allTriangles.data(), indexBufferSize);
    vkUnmapMemory(m_device, indexStagingMemory);

    // Create device local buffers
    createBuffer(vertexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_vertexBuffer, m_vertexBufferMemory);

    createBuffer(indexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_indexBuffer, m_indexBufferMemory);

    // Copy from staging to device local
    VkCommandBuffer cmdBuffer = m_commandBuffers[0];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer for mesh upload");
    }

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(cmdBuffer, vertexStagingBuffer, m_vertexBuffer, 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{};
    indexCopyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(cmdBuffer, indexStagingBuffer, m_indexBuffer, 1, &indexCopyRegion);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer for mesh upload");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    if (vkQueueSubmit(m_computeQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit mesh upload commands");
    }
    vkQueueWaitIdle(m_computeQueue);

    // Cleanup staging buffers
    vkDestroyBuffer(m_device, vertexStagingBuffer, nullptr);
    vkFreeMemory(m_device, vertexStagingMemory, nullptr);
    vkDestroyBuffer(m_device, indexStagingBuffer, nullptr);
    vkFreeMemory(m_device, indexStagingMemory, nullptr);

    // Create BVH buffers
    if (!allBvhNodes.empty()) {
        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       allBvhNodes, m_bvhNodeBuffer, m_bvhNodeBufferMemory);
        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       allBvhTriIndices, m_bvhTriIdxBuffer, m_bvhTriIdxBufferMemory);
    }

    // Create mesh info buffer
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   meshInfos, m_meshInfoBuffer, m_meshInfoBufferMemory);

    // Upload materials buffer
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   allMaterials, m_materialBuffer, m_materialBufferMemory);

    // Load and concatenate all textures
    std::vector<float> allTextureData;
    std::vector<Scene::GPUTextureInfo> textureInfos;
    uint32_t textureOffset = 0;

    for (const auto& texPath : texturePaths) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        Scene::GPUTextureInfo texInfo{};
        texInfo.offset = textureOffset;

        if (pixels) {
            texInfo.width = static_cast<uint32_t>(texWidth);
            texInfo.height = static_cast<uint32_t>(texHeight);

            // Convert to vec4 floats and append
            for (int i = 0; i < texWidth * texHeight; ++i) {
                allTextureData.push_back(pixels[i * 4 + 0] / 255.0f);  // R
                allTextureData.push_back(pixels[i * 4 + 1] / 255.0f);  // G
                allTextureData.push_back(pixels[i * 4 + 2] / 255.0f);  // B
                allTextureData.push_back(pixels[i * 4 + 3] / 255.0f);  // A
            }
            stbi_image_free(pixels);

            textureOffset += static_cast<uint32_t>(texWidth * texHeight);
        } else {
            std::cerr << "Warning: Failed to load texture: " << texPath << "\n";
            // Add a single white pixel as fallback
            texInfo.width = 1;
            texInfo.height = 1;
            allTextureData.push_back(1.0f);
            allTextureData.push_back(1.0f);
            allTextureData.push_back(1.0f);
            allTextureData.push_back(1.0f);
            textureOffset += 1;
        }

        textureInfos.push_back(texInfo);
    }

    // Ensure we have at least a dummy texture
    if (textureInfos.empty()) {
        Scene::GPUTextureInfo dummyInfo{};
        dummyInfo.offset = 0;
        dummyInfo.width = 1;
        dummyInfo.height = 1;
        textureInfos.push_back(dummyInfo);
        allTextureData = {1.0f, 1.0f, 1.0f, 1.0f};  // Single white pixel
    }

    // Upload texture data and info buffers
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   allTextureData, m_textureBuffer, m_textureBufferMemory);
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   textureInfos, m_textureInfoBuffer, m_textureInfoBufferMemory);

    // Store first texture dimensions for backwards compatibility with push constants
    m_textureWidth = textureInfos[0].width;
    m_textureHeight = textureInfos[0].height;

    m_meshesUploaded = true;
}

void VulkanRenderer::UploadTexture(const std::string& filename) {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (pixels) {
        m_textureWidth = static_cast<uint32_t>(texWidth);
        m_textureHeight = static_cast<uint32_t>(texHeight);

        // Convert to vec4 floats for shader (RGBA normalized)
        std::vector<float> textureData(texWidth * texHeight * 4);
        for (int i = 0; i < texWidth * texHeight; ++i) {
            textureData[i * 4 + 0] = pixels[i * 4 + 0] / 255.0f;  // R
            textureData[i * 4 + 1] = pixels[i * 4 + 1] / 255.0f;  // G
            textureData[i * 4 + 2] = pixels[i * 4 + 2] / 255.0f;  // B
            textureData[i * 4 + 3] = pixels[i * 4 + 3] / 255.0f;  // A
        }
        stbi_image_free(pixels);

        // Upload texture data to buffer
        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       textureData, m_textureBuffer, m_textureBufferMemory);

    } else {
        // Create dummy texture buffer
        std::vector<float> dummyTexture(4, 1.0f);  // Single white pixel
        m_textureWidth = 1;
        m_textureHeight = 1;
        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       dummyTexture, m_textureBuffer, m_textureBufferMemory);
    }
}

void VulkanRenderer::UploadSceneData(const Scene::SceneData& sceneData) {
    // Create sphere buffer
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   sceneData.spheres, m_sphereBuffer, m_sphereBufferMemory);

    // Create plane buffer
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   sceneData.planes, m_planeBuffer, m_planeBufferMemory);

    // Create light buffer
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   sceneData.lights, m_lightBuffer, m_lightBufferMemory);

    // Only create material buffer if not already uploaded by UploadMeshes
    // (UploadMeshes handles mesh materials with proper texture indices)
    if (m_materialBuffer == VK_NULL_HANDLE) {
        std::vector<Scene::GPUMaterial> gpuMaterials;
        if (!sceneData.materials.empty()) {
            gpuMaterials = sceneData.materials;
        } else {
            // Create default material if none provided (optimized 32-byte layout)
            Scene::GPUMaterial defaultMat{};
            defaultMat.diffuse[0] = 0.8f;
            defaultMat.diffuse[1] = 0.8f;
            defaultMat.diffuse[2] = 0.8f;
            defaultMat.shininess = 32.0f;
            defaultMat.shadingMode = static_cast<int32_t>(Scene::ShadingMode::PBR);
            defaultMat.diffuseTextureIndex = -1;
            defaultMat.metalness = 0.0f;
            defaultMat.roughness = 0.5f;
            gpuMaterials.push_back(defaultMat);
        }

        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       gpuMaterials, m_materialBuffer, m_materialBufferMemory);
    }

    // Create minimal instance buffer for descriptor set binding
    // The actual instance data will be uploaded separately via uploadInstances()
    std::vector<Scene::GPUMeshInstance> defaultInstances;
    defaultInstances.push_back(Scene::GPUMeshInstance::identity(0));  // Single identity instance
    VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                   defaultInstances, m_instanceMotorBuffer, m_instanceMotorBufferMemory);
    m_instanceBufferCapacity = 1;
}

void VulkanRenderer::UploadInstances(const std::vector<MeshInstance>& instances) {
    if (instances.empty()) {
        return;  // Keep default identity instance
    }

    const auto instanceCount = static_cast<uint32_t>(instances.size());

    // Check if we need to reallocate the buffer (instance count exceeds capacity)
    if (instanceCount > m_instanceBufferCapacity) {
        // Wait for GPU to finish using the old buffer
        vkDeviceWaitIdle(m_device);

        // Free old buffer
        if (m_instanceMotorBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_instanceMotorBuffer, nullptr);
            m_instanceMotorBuffer = VK_NULL_HANDLE;
        }
        if (m_instanceMotorBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_instanceMotorBufferMemory, nullptr);
            m_instanceMotorBufferMemory = VK_NULL_HANDLE;
        }
    }

    std::vector<Scene::GPUMeshInstance> gpuInstances;
    gpuInstances.reserve(instances.size());

    for (const auto& inst : instances) {
        Scene::GPUMeshInstance gpuInst{};
        gpuInst.meshId = inst.meshId;
        gpuInst.visible = inst.visible ? 1 : 0;

        // Convert PGA Motor to 4x4 transformation matrix
        // Motor = s + e01*d1 + e02*d2 + e03*d3 + e23*b1 + e31*b2 + e12*b3 + e0123*p
        // For translation + rotation motor:
        // Translation: d1=tx/2, d2=ty/2, d3=tz/2
        // Rotation: s=cos(angle/2), bivector part = sin(angle/2) * axis
        //
        // NOTE: PGA convention uses -sin(angle/2) for rotation (see Motor::Rotation in FlyFish)
        // The rotation matrix formulas use quaternion convention which expects +sin(angle/2)
        // So we NEGATE the bivector components for the ROTATION MATRIX only
        // The TRANSLATION formula needs the ORIGINAL PGA bivector values
        const Motor& m = inst.transform;
        float s = m.s();

        // Original PGA bivector values (for translation formula)
        float b1_pga = m.e23();
        float b2_pga = m.e31();
        float b3_pga = m.e12();

        // Negated bivector values (for quaternion-style rotation matrix)
        float b1 = -b1_pga;
        float b2 = -b2_pga;
        float b3 = -b3_pga;

        float d1 = m.e01();   // Translation X / 2
        float d2 = m.e02();   // Translation Y / 2
        float d3 = m.e03();   // Translation Z / 2
        float p = m.e0123();

        // Rotation matrix from motor (rotor part) - uses NEGATED bivector (quaternion convention)
        // R = I - 2*b^2 + 2*s*b  (quaternion-like formula)
        // For normalized motor, s^2 + b1^2 + b2^2 + b3^2 = 1
        float r00 = 1 - 2*(b2*b2 + b3*b3);
        float r01 = 2*(b1*b2 - s*b3);
        float r02 = 2*(b1*b3 + s*b2);
        float r10 = 2*(b1*b2 + s*b3);
        float r11 = 1 - 2*(b1*b1 + b3*b3);
        float r12 = 2*(b2*b3 - s*b1);
        float r20 = 2*(b1*b3 - s*b2);
        float r21 = 2*(b2*b3 + s*b1);
        float r22 = 1 - 2*(b1*b1 + b2*b2);

        // Translation from motor - uses ORIGINAL PGA bivector values
        // t = 2 * (s*d - d×b + p*b) where × is cross product
        // Note: the cross product is SUBTRACTED, not added
        float tx = 2 * (d1*s - d2*b3_pga + d3*b2_pga + p*b1_pga);
        float ty = 2 * (d2*s - d3*b1_pga + d1*b3_pga + p*b2_pga);
        float tz = 2 * (d3*s - d1*b2_pga + d2*b1_pga + p*b3_pga);

        // Apply uniform scale to transform
        float scl = inst.scale;
        float invScl = 1.0f / scl;

        // Build column-major 4x4 transform matrix (with scale applied to rotation)
        // Column 0 (indices 0-3)
        gpuInst.transform[0] = r00 * scl;
        gpuInst.transform[1] = r10 * scl;
        gpuInst.transform[2] = r20 * scl;
        gpuInst.transform[3] = 0.0f;
        // Column 1 (indices 4-7)
        gpuInst.transform[4] = r01 * scl;
        gpuInst.transform[5] = r11 * scl;
        gpuInst.transform[6] = r21 * scl;
        gpuInst.transform[7] = 0.0f;
        // Column 2 (indices 8-11)
        gpuInst.transform[8] = r02 * scl;
        gpuInst.transform[9] = r12 * scl;
        gpuInst.transform[10] = r22 * scl;
        gpuInst.transform[11] = 0.0f;
        // Column 3 (indices 12-15) - translation
        gpuInst.transform[12] = tx;
        gpuInst.transform[13] = ty;
        gpuInst.transform[14] = tz;
        gpuInst.transform[15] = 1.0f;

        // Build inverse transform (transpose of rotation with inverse scale)
        // For scaled rotation: (S*R)^-1 = R^T * S^-1
        // Column 0
        gpuInst.invTransform[0] = r00 * invScl;
        gpuInst.invTransform[1] = r01 * invScl;
        gpuInst.invTransform[2] = r02 * invScl;
        gpuInst.invTransform[3] = 0.0f;
        // Column 1
        gpuInst.invTransform[4] = r10 * invScl;
        gpuInst.invTransform[5] = r11 * invScl;
        gpuInst.invTransform[6] = r12 * invScl;
        gpuInst.invTransform[7] = 0.0f;
        // Column 2
        gpuInst.invTransform[8] = r20 * invScl;
        gpuInst.invTransform[9] = r21 * invScl;
        gpuInst.invTransform[10] = r22 * invScl;
        gpuInst.invTransform[11] = 0.0f;
        // Column 3 - inverse translation: -(S*R)^-1 * t = -(1/s) * R^T * t
        float invTx = -(r00*tx + r10*ty + r20*tz) * invScl;
        float invTy = -(r01*tx + r11*ty + r21*tz) * invScl;
        float invTz = -(r02*tx + r12*ty + r22*tz) * invScl;
        gpuInst.invTransform[12] = invTx;
        gpuInst.invTransform[13] = invTy;
        gpuInst.invTransform[14] = invTz;
        gpuInst.invTransform[15] = 1.0f;

        gpuInstances.push_back(gpuInst);
    }

    // Upload to GPU - use updateBufferData if buffer already exists (avoids invalidating descriptor set)
    if (m_instanceMotorBuffer != VK_NULL_HANDLE) {
        VulkanHelpers::updateBufferData(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                        gpuInstances, m_instanceMotorBuffer);
    } else {
        // Buffer was reallocated - create new buffer and update descriptor set
        VulkanHelpers::uploadToBuffer(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                       gpuInstances, m_instanceMotorBuffer, m_instanceMotorBufferMemory);

        // Track the new buffer capacity
        m_instanceBufferCapacity = instanceCount;

        // Update descriptor set to point to new buffer (binding 10)
        // Only if descriptor set already exists (not during initial setup)
        if (m_descriptorSet != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo instanceMotorBufferInfo{};
            instanceMotorBufferInfo.buffer = m_instanceMotorBuffer;
            instanceMotorBufferInfo.offset = 0;
            instanceMotorBufferInfo.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSet;
            descriptorWrite.dstBinding = 10;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &instanceMotorBufferInfo;

            vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
        }
    }

}

void VulkanRenderer::UpdateSpheres(const std::vector<Scene::GPUSphere>& spheres) {
    if (spheres.empty() || m_sphereBuffer == VK_NULL_HANDLE) {
        return;
    }
    VulkanHelpers::updateBufferData(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                    spheres, m_sphereBuffer);
}

void VulkanRenderer::UpdatePlanes(const std::vector<Scene::GPUPlane>& planes) {
    if (planes.empty() || m_planeBuffer == VK_NULL_HANDLE) {
        return;
    }
    VulkanHelpers::updateBufferData(m_device, m_physicalDevice, m_commandPool, m_computeQueue,
                                    planes, m_planeBuffer);
}

void VulkanRenderer::WaitIdle() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

// ============================================================================
// Core Initialization Methods
// ============================================================================

void VulkanRenderer::createInstance() {
    // Check for validation layer support if enabled
    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
        std::cerr << "Warning: Validation layers requested but not available!\n";
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "FlyTracer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Get SDL required extensions
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (sdlExtensions == nullptr) {
        throw std::runtime_error("Failed to get SDL Vulkan extensions: " + std::string(SDL_GetError()));
    }

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

    // Add debug utils extension for validation layer messages
    if (ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        // Add layer settings extension for configuring validation features
        extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
    }

#ifdef __APPLE__
    // Required for MoltenVK on macOS
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    // Setup validation features
    VkValidationFeatureEnableEXT enabledFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
    };

    // Disable best practices warnings (can be noisy)
    VkValidationFeatureDisableEXT disabledFeatures[] = {
        VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT  // Disable if too strict
    };

    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount = 0;
    validationFeatures.pEnabledValidationFeatures = enabledFeatures;
    validationFeatures.disabledValidationFeatureCount = 1;
    validationFeatures.pDisabledValidationFeatures = disabledFeatures;

    std::vector<const char*> enabledLayers;
    if (ENABLE_VALIDATION_LAYERS && checkValidationLayerSupport()) {
        std::cout << "Configuring Vulkan validation layers:\n";
        enabledLayers = getEnabledLayers();

        // Only enable GPU-assisted validation if environment variable is set
        const char* gpuAssistedEnv = std::getenv("FLYTRACER_GPU_VALIDATION");
        if (gpuAssistedEnv != nullptr && std::string(gpuAssistedEnv) == "1") {
            validationFeatures.enabledValidationFeatureCount = 2;  // Enable both features
            std::cout << "  GPU-assisted validation: ENABLED\n";
            std::cout << "  Debug printf: ENABLED\n";
        } else {
            validationFeatures.enabledValidationFeatureCount = 0;  // Disable GPU features by default
        }

        // Chain debug messenger info for instance creation/destruction messages
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        validationFeatures.pNext = &debugCreateInfo;
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
    createInfo.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();

    if (ENABLE_VALIDATION_LAYERS && !enabledLayers.empty()) {
        createInfo.pNext = &validationFeatures;
    }

#ifdef __APPLE__
    // Required flag for MoltenVK portability
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void VulkanRenderer::setupDebugMessenger() {
    if (!ENABLE_VALIDATION_LAYERS) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func == nullptr || func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }
}

void VulkanRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Candidate device info
    struct DeviceCandidate {
        VkPhysicalDevice device;
        uint32_t computeQueueFamily;
        uint32_t graphicsQueueFamily;
        uint32_t presentQueueFamily;
        int score;
    };

    std::vector<DeviceCandidate> candidates;

    // Score each device
    for (const auto& device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // Find queue families
        bool foundCompute = false;
        bool foundGraphics = false;
        bool foundPresent = false;
        uint32_t computeFamily = 0, graphicsFamily = 0, presentFamily = 0;

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (!foundCompute && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                computeFamily = i;
                foundCompute = true;
            }
            if (!foundGraphics && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                graphicsFamily = i;
                foundGraphics = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (!foundPresent && presentSupport) {
                presentFamily = i;
                foundPresent = true;
            }
            if (foundCompute && foundGraphics && foundPresent) {
                break;
            }
        }

        if (!foundCompute || !foundGraphics || !foundPresent) {
            continue;  // Skip unsuitable devices
        }

        // Score based on device type (prefer discrete GPUs)
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        int score = 0;
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score = 1000;  // Strongly prefer dedicated graphics cards
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score = 100;   // Integrated GPUs as fallback
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                score = 50;    // Virtual GPUs (VMs)
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                score = 10;    // Software rasterization
                break;
            default:
                score = 1;
                break;
        }

        candidates.push_back({device, computeFamily, graphicsFamily, presentFamily, score});
    }

    if (candidates.empty()) {
        throw std::runtime_error("Failed to find suitable GPU");
    }

    // Select the device with the highest score
    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const DeviceCandidate& a, const DeviceCandidate& b) {
            return a.score < b.score;
        });

    m_physicalDevice = best->device;
    m_computeQueueFamily = best->computeQueueFamily;
    m_graphicsQueueFamily = best->graphicsQueueFamily;
    m_presentQueueFamily = best->presentQueueFamily;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

    const char* deviceTypeStr = "Unknown";
    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceTypeStr = "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceTypeStr = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceTypeStr = "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceTypeStr = "CPU"; break;
        default: break;
    }

    std::cout << "Selected GPU: " << properties.deviceName << " (" << deviceTypeStr << ")\n";
}

void VulkanRenderer::createDevice() {
    std::set<uint32_t> uniqueQueueFamilies = {
        m_computeQueueFamily,
        m_graphicsQueueFamily,
        m_presentQueueFamily
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Query physical device features before creating the device (required by validation)
    VkPhysicalDeviceFeatures deviceFeatures{};
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &deviceFeatures);

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,  // Modern Vulkan 1.3 dynamic rendering
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME   // Modern Vulkan 1.3 synchronization2
    };

#ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

    // Enable Vulkan 1.3 features: synchronization2 and dynamic rendering
    VkPhysicalDeviceSynchronization2Features sync2Features{};
    sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2Features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext = &sync2Features;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &dynamicRenderingFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pNext = &deviceFeatures2;  // Use VkPhysicalDeviceFeatures2 with pNext chain
    createInfo.pEnabledFeatures = nullptr;  // Must be null when using pNext features
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    std::cout << "Device created with Vulkan 1.3 features: synchronization2, dynamic_rendering\n";

    vkGetDeviceQueue(m_device, m_computeQueueFamily, 0, &m_computeQueue);
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);

    // Load extension function pointers for MoltenVK compatibility
    m_vkQueueSubmit2KHR = reinterpret_cast<PFN_vkQueueSubmit2KHR>(
        vkGetDeviceProcAddr(m_device, "vkQueueSubmit2KHR"));
    m_vkCmdPipelineBarrier2KHR = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(
        vkGetDeviceProcAddr(m_device, "vkCmdPipelineBarrier2KHR"));
    m_vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(
        vkGetDeviceProcAddr(m_device, "vkCmdBeginRenderingKHR"));
    m_vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(
        vkGetDeviceProcAddr(m_device, "vkCmdEndRenderingKHR"));

    if (!m_vkQueueSubmit2KHR || !m_vkCmdPipelineBarrier2KHR ||
        !m_vkCmdBeginRenderingKHR || !m_vkCmdEndRenderingKHR) {
        throw std::runtime_error("Failed to load Vulkan 1.3 extension functions");
    }
    std::cout << "Vulkan 1.3 extension functions loaded\n";
}

void VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount,
                                        formats.data());

    // Choose format
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }

    // Use FIFO for vsync (guaranteed to be available)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Determine extent
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(static_cast<uint32_t>(m_config.width),
                                 capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(m_config.height),
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Swapchain is used by compute (copy), graphics (ImGui), and present queues
    // Collect unique queue families that need access
    std::set<uint32_t> uniqueQueueFamilies = {
        m_computeQueueFamily,
        m_graphicsQueueFamily,
        m_presentQueueFamily
    };
    std::vector<uint32_t> queueFamilyIndices(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());

    if (queueFamilyIndices.size() > 1) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());

    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    std::cout << "Swapchain created (" << extent.width << "x" << extent.height
              << ", " << imageCount << " images, FIFO mode for vsync)\n";
}

void VulkanRenderer::createRenderResources() {
    std::cout << "Creating render resources...\n";

    // Create resources in the correct order
    createCommandPool();
    createStorageImage();
    createDescriptorSetLayout();
    createDescriptorPool();
    createSyncObjects();
    createSwapchainImageViews();
    createRenderPass();
    createFramebuffers();
    createImGuiResources();

    std::cout << "Render resources created\n";
}

// ============================================================================
// Resource Creation Helper Methods
// ============================================================================

void VulkanRenderer::createCommandPool() {
    std::cout << "Creating command pool...\n";

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_computeQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    // Allocate per-frame command buffers
    m_commandBuffers.resize(m_swapchainImages.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    std::cout << "Command pool created\n";
}

void VulkanRenderer::createStorageImage() {
    std::cout << "Transitioning storage image layout...\n";

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_config.width;
    imageInfo.extent.height = m_config.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_storageImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_storageImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_storageImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(m_device, m_storageImage, m_storageImageMemory, 0);

    m_storageImageView = VulkanHelpers::createImageView(m_device, m_storageImage,
                                                         VK_FORMAT_R8G8B8A8_UNORM);

    // Transition to GENERAL layout once at creation using synchronization2
    VkCommandBuffer cmdBuffer = m_commandBuffers[0];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer for storage image transition");
    }
    // Modern Vulkan 1.3: Use NONE instead of TOP_OF_PIPE for initial transition
    VulkanHelpers::transitionImageLayout2(cmdBuffer, m_storageImage,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                         VK_PIPELINE_STAGE_2_NONE,
                         VK_ACCESS_2_NONE,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_SHADER_WRITE_BIT,
                         m_vkCmdPipelineBarrier2KHR);
    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer for storage image transition");
    }

    // Modern Vulkan 1.3: Use vkQueueSubmit2
    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{};
    cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferSubmitInfo.commandBuffer = cmdBuffer;

    VkSubmitInfo2 submitInfo2{};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &cmdBufferSubmitInfo;

    if (m_vkQueueSubmit2KHR(m_computeQueue, 1, &submitInfo2, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit storage image layout transition");
    }
    vkQueueWaitIdle(m_computeQueue);

    m_storageImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    std::cout << "Storage image created\n";
}

void VulkanRenderer::createDescriptorSetLayout() {
    // 13 bindings: storage image, vertex, index, spheres, planes, lights, materials, bvhNodes, bvhTriIdx, texture, instanceMotors, meshInfos, textureInfos
    std::vector<VulkanHelpers::DescriptorBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Vertices
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Indices
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Spheres
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Planes
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Lights
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Materials
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // BVH nodes
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // BVH tri indices
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},  // Texture data
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT}, // Instance motors
        {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT}, // Mesh infos
        {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT}, // Texture infos
    };

    m_descriptorSetLayout = VulkanHelpers::createDescriptorSetLayout(m_device, bindings);
    std::cout << "Descriptor set layout created\n";
}

void VulkanRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 12;  // vertex, index, spheres, planes, lights, materials, bvhNodes, bvhTriIdx, texture, instanceMotors, meshInfos, textureInfos

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    std::cout << "Descriptor pool created\n";
}

void VulkanRenderer::CreateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Update all 13 descriptors
    std::array<VkWriteDescriptorSet, 13> descriptorWrites{};

    // Storage image (binding 0)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = m_storageImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    // Vertex buffer (binding 1)
    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = m_vertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &vertexBufferInfo;

    // Index buffer (binding 2)
    VkDescriptorBufferInfo indexBufferInfo{};
    indexBufferInfo.buffer = m_indexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &indexBufferInfo;

    // Sphere buffer (binding 3)
    VkDescriptorBufferInfo sphereBufferInfo{};
    sphereBufferInfo.buffer = m_sphereBuffer;
    sphereBufferInfo.offset = 0;
    sphereBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &sphereBufferInfo;

    // Plane buffer (binding 4)
    VkDescriptorBufferInfo planeBufferInfo{};
    planeBufferInfo.buffer = m_planeBuffer;
    planeBufferInfo.offset = 0;
    planeBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = m_descriptorSet;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &planeBufferInfo;

    // Light buffer (binding 5)
    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_lightBuffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = m_descriptorSet;
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &lightBufferInfo;

    // Material buffer (binding 6)
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = m_materialBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[6].dstSet = m_descriptorSet;
    descriptorWrites[6].dstBinding = 6;
    descriptorWrites[6].dstArrayElement = 0;
    descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[6].descriptorCount = 1;
    descriptorWrites[6].pBufferInfo = &materialBufferInfo;

    // BVH node buffer (binding 7)
    VkDescriptorBufferInfo bvhNodeBufferInfo{};
    bvhNodeBufferInfo.buffer = m_bvhNodeBuffer;
    bvhNodeBufferInfo.offset = 0;
    bvhNodeBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[7].dstSet = m_descriptorSet;
    descriptorWrites[7].dstBinding = 7;
    descriptorWrites[7].dstArrayElement = 0;
    descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[7].descriptorCount = 1;
    descriptorWrites[7].pBufferInfo = &bvhNodeBufferInfo;

    // BVH triangle index buffer (binding 8)
    VkDescriptorBufferInfo bvhTriIdxBufferInfo{};
    bvhTriIdxBufferInfo.buffer = m_bvhTriIdxBuffer;
    bvhTriIdxBufferInfo.offset = 0;
    bvhTriIdxBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[8].dstSet = m_descriptorSet;
    descriptorWrites[8].dstBinding = 8;
    descriptorWrites[8].dstArrayElement = 0;
    descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[8].descriptorCount = 1;
    descriptorWrites[8].pBufferInfo = &bvhTriIdxBufferInfo;

    // Texture buffer (binding 9)
    VkDescriptorBufferInfo textureBufferInfo{};
    textureBufferInfo.buffer = m_textureBuffer;
    textureBufferInfo.offset = 0;
    textureBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[9].dstSet = m_descriptorSet;
    descriptorWrites[9].dstBinding = 9;
    descriptorWrites[9].dstArrayElement = 0;
    descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[9].descriptorCount = 1;
    descriptorWrites[9].pBufferInfo = &textureBufferInfo;

    // Instance motor buffer (binding 10)
    VkDescriptorBufferInfo instanceMotorBufferInfo{};
    instanceMotorBufferInfo.buffer = m_instanceMotorBuffer != VK_NULL_HANDLE ? m_instanceMotorBuffer : m_vertexBuffer; // Use dummy buffer if not created
    instanceMotorBufferInfo.offset = 0;
    instanceMotorBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[10].dstSet = m_descriptorSet;
    descriptorWrites[10].dstBinding = 10;
    descriptorWrites[10].dstArrayElement = 0;
    descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[10].descriptorCount = 1;
    descriptorWrites[10].pBufferInfo = &instanceMotorBufferInfo;

    // Mesh info buffer (binding 11)
    VkDescriptorBufferInfo meshInfoBufferInfo{};
    meshInfoBufferInfo.buffer = m_meshInfoBuffer != VK_NULL_HANDLE ? m_meshInfoBuffer : m_vertexBuffer; // Use dummy buffer if not created
    meshInfoBufferInfo.offset = 0;
    meshInfoBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[11].dstSet = m_descriptorSet;
    descriptorWrites[11].dstBinding = 11;
    descriptorWrites[11].dstArrayElement = 0;
    descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[11].descriptorCount = 1;
    descriptorWrites[11].pBufferInfo = &meshInfoBufferInfo;

    // Texture info buffer (binding 12)
    VkDescriptorBufferInfo textureInfoBufferInfo{};
    textureInfoBufferInfo.buffer = m_textureInfoBuffer != VK_NULL_HANDLE ? m_textureInfoBuffer : m_vertexBuffer; // Use dummy buffer if not created
    textureInfoBufferInfo.offset = 0;
    textureInfoBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[12].dstSet = m_descriptorSet;
    descriptorWrites[12].dstBinding = 12;
    descriptorWrites[12].dstArrayElement = 0;
    descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[12].descriptorCount = 1;
    descriptorWrites[12].pBufferInfo = &textureInfoBufferInfo;

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
                          descriptorWrites.data(), 0, nullptr);

    std::cout << "Descriptor set created\n";
}

void VulkanRenderer::CreateComputePipeline() {
    const std::string shaderPath = m_config.shaderDir + "/raytracer.comp.spv";
    auto shaderCode = readFile(shaderPath);
    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr,
                              &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.stage = shaderStageInfo;

    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                 nullptr, &m_computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }

    vkDestroyShaderModule(m_device, shaderModule, nullptr);
    std::cout << "Compute pipeline created\n";
}

void VulkanRenderer::createSyncObjects() {
    const size_t imageCount = m_swapchainImages.size();
    m_imageAvailableSemaphores.resize(imageCount);
    m_computeFinishedSemaphores.resize(imageCount);
    m_renderFinishedSemaphores.resize(imageCount);
    m_inFlightFences.resize(imageCount);
    // Track which fence is currently using each swapchain image (initially none)
    m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                             &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                             &m_computeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                             &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    std::cout << "Sync objects created\n";
}

void VulkanRenderer::createSwapchainImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
        m_swapchainImageViews[i] = VulkanHelpers::createImageView(
            m_device, m_swapchainImages[i], m_swapchainImageFormat);
    }

    std::cout << "Swapchain image views created\n";
}

void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    std::cout << "Render pass created\n";
}

void VulkanRenderer::createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_swapchainImageViews[i];
        framebufferInfo.width = m_swapchainExtent.width;
        framebufferInfo.height = m_swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    std::cout << "Framebuffers created\n";
}

void VulkanRenderer::createImGuiResources() {
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // Create command pool for ImGui
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_graphicsQueueFamily;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_imguiCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui command pool");
    }

    // Allocate command buffers for ImGui
    m_imguiCommandBuffers.resize(m_swapchainImages.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_imguiCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_imguiCommandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_imguiCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate ImGui command buffers");
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(m_window);

    // Use traditional render pass instead of dynamic rendering (workaround for MoltenVK flickering)
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_graphicsQueueFamily;
    initInfo.Queue = m_graphicsQueue;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.RenderPass = m_renderPass;  // Use render pass instead of dynamic rendering
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo);

    std::cout << "ImGui initialized with render pass\n";
}

void VulkanRenderer::recreateSwapchain() {
    // Recreate swapchain (currently not implemented - window resizing not supported)
}

void VulkanRenderer::cleanup() {
    WaitIdle();

    // Shutdown ImGui BEFORE destroying any Vulkan resources it depends on
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // Cleanup swapchain resources
    cleanupSwapchain();

    // Destroy all resources BEFORE destroying the device
    if (m_device != VK_NULL_HANDLE) {
        // Destroy scene buffers
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
        if (m_indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_bvhNodeBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_bvhNodeBuffer, nullptr);
            m_bvhNodeBuffer = VK_NULL_HANDLE;
        }
        if (m_bvhNodeBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_bvhNodeBufferMemory, nullptr);
            m_bvhNodeBufferMemory = VK_NULL_HANDLE;
        }
        if (m_bvhTriIdxBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_bvhTriIdxBuffer, nullptr);
            m_bvhTriIdxBuffer = VK_NULL_HANDLE;
        }
        if (m_bvhTriIdxBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_bvhTriIdxBufferMemory, nullptr);
            m_bvhTriIdxBufferMemory = VK_NULL_HANDLE;
        }
        if (m_meshInfoBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_meshInfoBuffer, nullptr);
            m_meshInfoBuffer = VK_NULL_HANDLE;
        }
        if (m_meshInfoBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_meshInfoBufferMemory, nullptr);
            m_meshInfoBufferMemory = VK_NULL_HANDLE;
        }
        if (m_sphereBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_sphereBuffer, nullptr);
            m_sphereBuffer = VK_NULL_HANDLE;
        }
        if (m_sphereBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_sphereBufferMemory, nullptr);
            m_sphereBufferMemory = VK_NULL_HANDLE;
        }
        if (m_planeBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_planeBuffer, nullptr);
            m_planeBuffer = VK_NULL_HANDLE;
        }
        if (m_planeBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_planeBufferMemory, nullptr);
            m_planeBufferMemory = VK_NULL_HANDLE;
        }
        if (m_lightBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_lightBuffer, nullptr);
            m_lightBuffer = VK_NULL_HANDLE;
        }
        if (m_lightBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_lightBufferMemory, nullptr);
            m_lightBufferMemory = VK_NULL_HANDLE;
        }
        if (m_materialBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_materialBuffer, nullptr);
            m_materialBuffer = VK_NULL_HANDLE;
        }
        if (m_materialBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_materialBufferMemory, nullptr);
            m_materialBufferMemory = VK_NULL_HANDLE;
        }
        if (m_textureBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_textureBuffer, nullptr);
            m_textureBuffer = VK_NULL_HANDLE;
        }
        if (m_textureBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_textureBufferMemory, nullptr);
            m_textureBufferMemory = VK_NULL_HANDLE;
        }
        if (m_textureInfoBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_textureInfoBuffer, nullptr);
            m_textureInfoBuffer = VK_NULL_HANDLE;
        }
        if (m_textureInfoBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_textureInfoBufferMemory, nullptr);
            m_textureInfoBufferMemory = VK_NULL_HANDLE;
        }
        if (m_instanceMotorBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_instanceMotorBuffer, nullptr);
            m_instanceMotorBuffer = VK_NULL_HANDLE;
        }
        if (m_instanceMotorBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_instanceMotorBufferMemory, nullptr);
            m_instanceMotorBufferMemory = VK_NULL_HANDLE;
        }

        // Destroy storage image resources
        if (m_storageImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_storageImageView, nullptr);
            m_storageImageView = VK_NULL_HANDLE;
        }
        if (m_storageImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_storageImage, nullptr);
            m_storageImage = VK_NULL_HANDLE;
        }
        if (m_storageImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_storageImageMemory, nullptr);
            m_storageImageMemory = VK_NULL_HANDLE;
        }

        // Destroy compute pipeline
        if (m_computePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_computePipeline, nullptr);
            m_computePipeline = VK_NULL_HANDLE;
        }
        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }

        // Destroy descriptor resources
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }

        // Destroy sync objects
        for (auto& semaphore : m_imageAvailableSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        m_imageAvailableSemaphores.clear();
        for (auto& semaphore : m_computeFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        m_computeFinishedSemaphores.clear();
        for (auto& semaphore : m_renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        m_renderFinishedSemaphores.clear();
        for (auto& fence : m_inFlightFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device, fence, nullptr);
            }
        }
        m_inFlightFences.clear();

        // Destroy command pools
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        if (m_imguiCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_imguiCommandPool, nullptr);
            m_imguiCommandPool = VK_NULL_HANDLE;
        }

        // Destroy ImGui descriptor pool
        if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
            m_imguiDescriptorPool = VK_NULL_HANDLE;
        }

        // Destroy render pass
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }

        // Destroy framebuffers
        for (auto framebuffer : m_framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_device, framebuffer, nullptr);
            }
        }
        m_framebuffers.clear();
    }

    // Destroy swapchain
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    // Destroy logical device (after all device resources are cleaned up)
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::cleanupSwapchain() {
    // Cleanup swapchain image views
    for (auto imageView : m_swapchainImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
    }
    m_swapchainImageViews.clear();
}

// ============================================================================
// Helper Methods
// ============================================================================

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties,
                                   VkBuffer& buffer, VkDeviceMemory& bufferMemory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

std::vector<char> VulkanRenderer::readFile(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}
