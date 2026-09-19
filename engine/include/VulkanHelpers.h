#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <utility>

namespace VulkanHelpers {

// Buffer creation helper
inline void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX) {
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

// Upload data to a device-local buffer using staging
template<typename T>
void uploadToBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<T>& data,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    if (data.empty()) {
        createBuffer(device, physicalDevice, sizeof(T),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    buffer, bufferMemory);
        return;
    }

    const VkDeviceSize bufferSize = sizeof(T) * data.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);

    void* mappedData = nullptr;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);
    std::memcpy(mappedData, data.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingMemory);

    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                buffer, bufferMemory);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(cmdBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

// Update data in an existing buffer
template<typename T>
void updateBufferData(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue queue,
    const std::vector<T>& data,
    VkBuffer buffer)
{
    if (data.empty() || buffer == VK_NULL_HANDLE) {
        return;
    }

    const VkDeviceSize bufferSize = sizeof(T) * data.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);

    void* mappedData = nullptr;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &mappedData);
    std::memcpy(mappedData, data.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingMemory);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(cmdBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

// Image layout transition using VK_KHR_synchronization2 (Vulkan 1.3)
inline void transitionImageLayout2(
    VkCommandBuffer cmdBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 dstAccess,
    PFN_vkCmdPipelineBarrier2KHR pfnCmdPipelineBarrier2KHR)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    pfnCmdPipelineBarrier2KHR(cmdBuffer, &depInfo);
}

// Descriptor set layout binding helper
struct DescriptorBinding {
    uint32_t binding;
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;
    uint32_t count{1};
};

[[nodiscard]] inline VkDescriptorSetLayout createDescriptorSetLayout(
    VkDevice device,
    const std::vector<DescriptorBinding>& bindings)
{
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    layoutBindings.reserve(bindings.size());

    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = b.binding;
        layoutBinding.descriptorType = b.type;
        layoutBinding.descriptorCount = b.count;
        layoutBinding.stageFlags = b.stageFlags;
        layoutBindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    return layout;
}

// Shader module creation helper
[[nodiscard]] inline VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

// Image view creation helper
[[nodiscard]] inline VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

// RAII wrapper for Vulkan buffer
class Buffer {
public:
    Buffer() noexcept = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
        : m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
        , m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
        , m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE))
        , m_size(std::exchange(other.m_size, 0)) {}

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
            m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
            m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
            m_size = std::exchange(other.m_size, 0);
        }
        return *this;
    }

    ~Buffer() { cleanup(); }

    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties) {
        cleanup();
        m_device = device;
        m_size = size;
        createBuffer(device, physicalDevice, size, usage, properties, m_buffer, m_memory);
    }

    void cleanup() noexcept {
        if (m_device != VK_NULL_HANDLE) {
            if (m_buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device, m_buffer, nullptr);
                m_buffer = VK_NULL_HANDLE;
            }
            if (m_memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, m_memory, nullptr);
                m_memory = VK_NULL_HANDLE;
            }
            m_device = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkBuffer buffer() const noexcept { return m_buffer; }
    [[nodiscard]] VkDeviceMemory memory() const noexcept { return m_memory; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return m_size; }
    [[nodiscard]] bool valid() const noexcept { return m_buffer != VK_NULL_HANDLE; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    VkDeviceSize m_size{0};
};

} // namespace VulkanHelpers
