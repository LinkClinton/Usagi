﻿#pragma once

namespace usagi
{
/**
 * \brief Specify the types of source data in buffers.
 */
enum class GpuDataFormat
{
    R32_SFLOAT,
    R32G32_SFLOAT,
    R32G32B32_SFLOAT,
    R32G32B32A32_SFLOAT,
};
}