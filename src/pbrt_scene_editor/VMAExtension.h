//
// Created by 王泽远 on 2024/1/21.
//

#ifndef PBRTEDITOR_VMAEXTENSION_H
#define PBRTEDITOR_VMAEXTENSION_H

#include "VulkanExtension.h"

struct VMABuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

/*
 * Remember, mapped buffer provide a "direct connection"
 * between the CPU memory and GPU memory.
 * Once you update the data on CPU side, the data on GPU side
 * will automatically observe the change.
 *
 * Which means, one should take care of
 * the "frame in flight" synchronization problem, i.e. by creating
 * multiple data objects when multiple frames recording is allowed.
 *
 * This type of memory is small, and might be not as efficient as device local type.
 * It's suitable for frequently update data on both CPU/GPU side.
 * */
template<typename T>
struct VMAObservedBufferMapped
{
    T* operator->() {
        return static_cast<T*>(allocationInfo.pMappedData);
    }

    VkBuffer getBuffer() const
    {
        return buffer;
    }

    friend struct DeviceExtended;
private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo{};
};

/*
 * This type of buffer implement another model for host/device communication.
 * Host only maintains a single copy of data. When the data is modified, GPU will not
 * automatically see the change. To make the change visible to GPU, one must explicitly call push.
 * Implementation can therefore choose device local memory, which is more efficient for shader access.
 *
 * Also, one needs to indicate an index of buffer push to. This is also for frames in flight use.
 * For instance, when GPU is consuming the buffer 0, is safe to modify the data at CPU side for next frame use,
 * push it to idx 1.
 * Just make sure when you want to push to 0 again, buffer 0 has been out of usage.
 * */
template<typename T>
struct VMAObservedBufferPush
{
    explicit VMAObservedBufferPush(int num_dev)
    {

    }

    T* operator->() {
        return &_host_data;
    }

    void push(int idx)
    {
        if(idx >= num_dev)
            return;

    }
private:
    int num_dev{};
    T _host_data;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

struct VMAImage
{
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo{};
};

#endif //PBRTEDITOR_VMAEXTENSION_H
