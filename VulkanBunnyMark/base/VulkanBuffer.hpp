/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>

#include "VulkanDevice.hpp"
#include "VulkanTools.h"
#include "volk/volk.h"

namespace vks {

enum BufferType { device, transient, staging };

struct Buffer {
    vks::VulkanDevice* vdevice;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation;
    VkDeviceSize size = 0;
    void* mappedData = nullptr;
    bool isPersistentMapped = false;

    VkDescriptorBufferInfo descriptor;

    VkBufferUsageFlags bufferUsage;
    BufferType bufferType;
    VkMemoryPropertyFlags memoryFlags;

    void create(vks::VulkanDevice* vulkanDevice, BufferType bufferType, VkBufferUsageFlags usage, VkDeviceSize size, bool persistentMapped = false)
    {
        if (buffer) destroy();
        vdevice = vulkanDevice;
        isPersistentMapped = persistentMapped;

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocCreateInfo = {};
        switch (bufferType) {
        case BufferType::device:
            allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // to detect unified memory
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case BufferType::staging:
            allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case BufferType::transient:
            allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        }
        if (isPersistentMapped) {
            allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        VmaAllocationInfo allocInfo;
        VK_CHECK(vmaCreateBuffer(vdevice->allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, &allocInfo));
        vmaGetMemoryTypeProperties(vdevice->allocator, allocInfo.memoryType, &memoryFlags);
        if (isPersistentMapped) {
            mappedData = allocInfo.pMappedData;
        }
        descriptor = { buffer, 0, size };
    }

    void* map()
    {
        assert(buffer && (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0);
        if (!mappedData) {
            VK_CHECK(vmaMapMemory(vdevice->allocator, allocation, &mappedData));
        }
        return mappedData;
    }

    void unmap()
    {
        if (mappedData && !isPersistentMapped) {
            vmaUnmapMemory(vdevice->allocator, allocation);
            mappedData = nullptr;
        }
    }

    void upload(void* data, VkDeviceSize size) {
        map();
        memcpy(mappedData, data, size);
        unmap();
    }

    void uploadFromStaging(void* data, VkDeviceSize size, VkQueue copyQueue) {
        if ((memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
            upload(data, size);
            return;
        }

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        VkBuffer sbuffer;
        VmaAllocation sallocation;
        VK_CHECK(vmaCreateBuffer(vdevice->allocator, &bufferInfo, &allocInfo, &sbuffer, &sallocation, nullptr));

        void* mapped;
        vmaMapMemory(vdevice->allocator, sallocation, &mapped);
        memcpy(mapped, data, size);
        vmaUnmapMemory(vdevice->allocator, sallocation);

        VkCommandBuffer copyCmd = vdevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(copyCmd, sbuffer, buffer, 1, &copyRegion);
        vdevice->flushCommandBuffer(copyCmd, copyQueue);

        vmaDestroyBuffer(vdevice->allocator, sbuffer, sallocation);
    }

    void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        assert(buffer);
        vmaFlushAllocation(vdevice->allocator, allocation, offset, size);
    }

    void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
    {
        assert(buffer);
        vmaInvalidateAllocation(vdevice->allocator, allocation, offset, size);
    }

    void destroy()
    {
        if (buffer) {
            unmap();
            vmaDestroyBuffer(vdevice->allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
        }
    }
};
}