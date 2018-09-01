﻿#pragma once

#include <memory>

#include <Usagi/Utility/Noncopyable.hpp>
#include <Usagi/Core/Math.hpp>

namespace usagi
{
struct GpuImageViewCreateInfo;
class GpuImageView;

class GpuImage : Noncopyable
{
public:
    virtual ~GpuImage() = default;

    virtual Vector2u32 size() const = 0;
    virtual std::shared_ptr<GpuImageView> baseView() = 0;
    virtual std::shared_ptr<GpuImageView> createView(
        const GpuImageViewCreateInfo &info) = 0;

    /**
     * \brief Upload image data to GPU memory.
     * \param data
     * \param size
     */
    virtual void upload(const void *data, std::size_t size) = 0;
};
}