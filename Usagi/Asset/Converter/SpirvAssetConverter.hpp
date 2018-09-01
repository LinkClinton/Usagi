﻿#pragma once

#include <memory>
#include <filesystem>
#include <optional>

#include <Usagi/Asset/Decoder/RawAssetDecoder.hpp>
#include <Usagi/Runtime/Graphics/Shader/SpirvBinary.hpp>

namespace usagi
{
struct SpirvAssetConverter
{
    using DefaultDecoder = RawAssetDecoder;

    std::shared_ptr<SpirvBinary> operator()(
        std::istream &in,
        ShaderStage stage,
        const std::optional<std::filesystem::path> & cache_folder =
            "./.cache/shader"
    ) const;
};
}