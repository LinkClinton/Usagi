﻿#pragma once

#include <vector>

#include "Enum/GpuBufferFormat.hpp"
#include "Enum/GpuImageLayout.hpp"

namespace usagi
{
class GpuImage;

enum class GpuAttachmentLoadOp
{
    LOAD,
    CLEAR,
    UNDEFINED
};

enum class GpuAttachmentStoreOp
{
    STORE,
    UNDEFINED
};

struct GpuAttachmentUsage
{
    const GpuBufferFormat format;
    const std::uint16_t sample_count;

    /**
    * \brief The layout before entering any the render pass.
    */
    const GpuImageLayout initial_layout;

    /**
    * \brief The layout used during the render pass.
    */
    GpuImageLayout layout = GpuImageLayout::COLOR;

    /**
    * \brief The layout after the render pass.
    */
    const GpuImageLayout final_layout;

    GpuAttachmentLoadOp load_op = GpuAttachmentLoadOp::LOAD;
    GpuAttachmentStoreOp store_op = GpuAttachmentStoreOp::STORE;

    GpuAttachmentUsage(
        const GpuBufferFormat format,
        const std::uint16_t sample_count,
        const GpuImageLayout initial_layout,
        const GpuImageLayout final_layout)
        : format { format }
        , sample_count { sample_count }
        , initial_layout { initial_layout }
        , final_layout { final_layout }
    {
    }
};

struct RenderPassCreateInfo
{
    // attachment indices must be sequential.
    std::vector<GpuAttachmentUsage> attachment_usages;
};
}
