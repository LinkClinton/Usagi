﻿#pragma once

#include <vulkan/vulkan.hpp>

#include <Usagi/Engine/Runtime/Graphics/Resource/MemoryAllocator.hpp>

namespace yuki
{

class MemoryAllocator;

}

namespace yuki::vulkan
{

class MemoryAllocator : public graphics::MemoryAllocator
{
    vk::Device mDevice;
    vk::UniqueBuffer mBuffer;
    vk::UniqueDeviceMemory mMemory;
    size_t mSize, mAlignment;
    std::unique_ptr<yuki::MemoryAllocator> mAllocator;
    void *mMappedAddress = nullptr;

public:
    MemoryAllocator(
        size_t size,
        vk::Device device,
        vk::PhysicalDevice physical_device,
        std::unique_ptr<yuki::MemoryAllocator> allocator
    );
    virtual ~MemoryAllocator();

    Allocation allocate(size_t size) override;
    void release(const Allocation &alloc_info) override;
    // this allocator does not actually reallocate. it just returns the original one.
    // todo: what if the allocator is reset? define a clear semantic.
    Allocation reallocate(const Allocation &alloc_info) override;
    void reset() override;

    void * getMappedAddress(const Allocation &allocation) const override;
    void flushRange(const Allocation &allocation, size_t offset, size_t size) override;

    graphics::MemoryAllocator * getActualAllocator(const Allocation &allocation) override;

    vk::Buffer _getBuffer() const;
};

}