﻿#include <Usagi/Engine/Runtime/GraphicsDevice/GraphicsDevice.hpp>
#include <Usagi/Engine/Runtime/GraphicsDevice/GDPipeline.hpp>
#include <Usagi/Engine/Runtime/GraphicsDevice/VertexBuffer.hpp>

#include "DebugFullscreenTriangle.hpp"
#include <Usagi/Engine/Runtime/GraphicsDevice/Shader.hpp>

namespace
{

const char *triangle_vs = R"(
#version 450 core
layout(location = 0) in vec3 vertexPosition_modelspace;
void main(){
  gl_Position.xyz = vertexPosition_modelspace;
  gl_Position.w = 1.0;
}
)";

const char *triangle_fs = R"(
#version 450 core
out vec3 color;
void main(){
  color = vec3(1,0,0);
}
)";

}

yuki::DebugFullscreenTriangle::DebugFullscreenTriangle(std::shared_ptr<GraphicsDevice> graphics_device)
    : Renderable { std::move(graphics_device) }
{
    mPipeline = mGraphicsDevice->createPipeline();
    mPipeline->vpSetVertexInputFormat({
        { 0, 0, 0, NativeDataType::FLOAT, 3, false }, // vertexPosition_modelspace
    });
    {
        auto vb = mGraphicsDevice->createVertexBuffer();
        vb->initStorage(3, sizeof(float) * 3);
        static const float vertex_buffer_data[] = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
        };
        vb->streamFromHostBuffer(vertex_buffer_data, sizeof(vertex_buffer_data));
        mPipeline->vpBindVertexBuffer(0, std::move(vb));
    }
    {
        auto vs = mGraphicsDevice->createVertexShader();
        vs->useSourceString(triangle_vs);
        vs->compile();
        mPipeline->vsUseVertexShader(std::move(vs));
    }
    {
        auto fs = mGraphicsDevice->createFragmentShader();
        fs->useSourceString(triangle_fs);
        fs->compile();
        mPipeline->fsUseFragmentShader(std::move(fs));
    }
}

void yuki::DebugFullscreenTriangle::render(const Clock &render_clock)
{
    mPipeline->assemble();
    mGraphicsDevice->drawTriangles(0, 3);
}
