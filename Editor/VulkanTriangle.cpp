﻿#include <Usagi/Engine/Runtime/Graphics/GraphicsPipeline.hpp>
#include <Usagi/Engine/Runtime/Graphics/GraphicsDevice2.hpp>
#include <Usagi/Engine/Runtime/Graphics/SPIRVShader.hpp>
#include <Usagi/Engine/Runtime/Graphics/GraphicsEnvironment.hpp>
#include <Usagi/Engine/Runtime/Graphics/SwapChain.hpp>
#include <Usagi/Engine/Runtime/Graphics/GraphicsCommandList.hpp>
#include <Usagi/Engine/Runtime/Graphics/FrameController.hpp>
#include <Usagi/Engine/Runtime/Graphics/GraphicsResourceAllocator.hpp>
#include <Usagi/Engine/Runtime/Graphics/GraphicsBuffer.hpp>

#include "VulkanTriangle.hpp"

yuki::VulkanTriangle::VulkanTriangle(GraphicsEnvironment *env)
    : mEnv { env }
{
    mPipeline = env->graphics_device->createGraphicsPipeline();
    GraphicsPipelineCreateInfo graphics_pipeline_create_info;
    graphics_pipeline_create_info.vertex_input.bindings.push_back({ 0, sizeof(VertexData) });
    graphics_pipeline_create_info.vertex_input.attributes.push_back({ 0, 0, GraphicsBufferElementType::R32G32B32A32_SFLOAT, 0 });
    graphics_pipeline_create_info.vertex_input.attributes.push_back({ 1, 0, GraphicsBufferElementType::R32G32B32A32_SFLOAT, 4 * sizeof(float) });
    graphics_pipeline_create_info.vertex_shader = SPIRVShader::readFromFile(R"(D:\Development\IntroductionToVulkan\Project\Tutorial04\Data04\vert.spv)");
    graphics_pipeline_create_info.fragment_shader = SPIRVShader::readFromFile(R"(D:\Development\IntroductionToVulkan\Project\Tutorial04\Data04\frag.spv)");
    graphics_pipeline_create_info.input_assembly.topology = InputAssembleState::PrimitiveTopology::TRIANGLE_STRIP;
    graphics_pipeline_create_info.rasterization.face_culling_mode = RasterizationState::FaceCullingMode::BACK;
    graphics_pipeline_create_info.rasterization.front_face = RasterizationState::FrontFace::COUNTER_CLOCKWISE;
    graphics_pipeline_create_info.rasterization.polygon_mode = RasterizationState::PolygonMode::FILL;
    graphics_pipeline_create_info.depth_stencil.enable_depth_test = false;
    graphics_pipeline_create_info.color_blend.enable = false;
    // todo: Renderable shouldn't know the actual render targets, as they are managed on the application layer
    graphics_pipeline_create_info.attachment_usages.push_back({ env->swap_chain->getNativeImageFormat(), GraphicsImageLayout::UNDEFINED, GraphicsImageLayout::PRESENT_SRC });
    mPipeline->create(graphics_pipeline_create_info);

    mVertexBuffer = env->frame_control->getResourceAllocator()->createDynamicBuffer(sizeof(VertexData) * 4);
}

yuki::VulkanTriangle::~VulkanTriangle()
{
}

void yuki::VulkanTriangle::update(double delta_time)
{
    mCounter1 += 0.1f * delta_time;
    mCounter1 = fmodf(mCounter1, 1.f);

    VertexData vertex_data[]
    {
        {
            -0.7f, -0.7f, 0.0f, 1.0f,
            mCounter1, 0.0f, 0.0f, 0.0f
        },
        {
            -0.7f, 0.7f, 0.0f, 1.0f,
            0.0f, mCounter1, 0.0f, 0.0f
        },
        {
            0.7f, -0.7f, 0.0f, 1.0f,
            0.0f, 0.0f, mCounter1, 0.0f
        },
        {
            0.7f, 0.7f, 0.0f, 1.0f,
            0.3f, mCounter1, 0.3f, 0.0f
        }
    };
    // since the old content may still be used by the device, we does not directly release
    // the memory. instead a new chuck is requested from the allocator.
    mVertexBuffer->reallocate();
    // the buffer is persistently mapped
    memcpy(mVertexBuffer->getMappedAddress(), vertex_data, sizeof(vertex_data));
    mVertexBuffer->flush();
}

void yuki::VulkanTriangle::populateCommandList(GraphicsCommandList *command_list)
{
    command_list->bindPipeline(mPipeline.get());
    command_list->setScissor(0, 0, 1280, 720);
    command_list->setViewport(0, 0, 1280, 720);
    command_list->bindVertexBuffer(0, mVertexBuffer.get());
    command_list->draw(4, 1, 0, 0);
}
