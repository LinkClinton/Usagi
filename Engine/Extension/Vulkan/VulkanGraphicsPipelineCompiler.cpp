﻿#include "VulkanGraphicsPipelineCompiler.hpp"

#include <loguru.hpp>

#include <Usagi/Engine/Runtime/Graphics/Shader/SpirvBinary.hpp>
#include <Usagi/Engine/Utility/TypeCast.hpp>

#include "VulkanGpuDevice.hpp"
#include "VulkanGraphicsPipeline.hpp"
#include "VulkanEnumTranslation.hpp"
#include "VulkanRenderPass.hpp"

using namespace spirv_cross;

vk::UniqueShaderModule usagi::VulkanGraphicsPipelineCompiler::
    createShaderModule(const SpirvBinary *binary) const
{
    auto &bytecodes = binary->bytecodes();
	vk::ShaderModuleCreateInfo create_info;
	create_info.setCodeSize(bytecodes.size());
	create_info.setPCode(bytecodes.data());
	return mDevice->device().createShaderModuleUnique(create_info);
}

void usagi::VulkanGraphicsPipelineCompiler::setupShaderStages()
{
	LOG_IF_F(ERROR, mShaders.count(ShaderStage::VERTEX) == 0,
		"Pipeline does not contain vertex stage.");
	LOG_IF_F(WARNING, mShaders.count(ShaderStage::FRAGMENT) == 0,
		"Pipeline does not contain fragment stage.");

    std::vector<vk::PipelineShaderStageCreateInfo> create_infos;
    for(auto &&shader : mShaders)
    {
        LOG_SCOPE_F(INFO, "Adding %s stage", to_string(shader.first));

        vk::PipelineShaderStageCreateInfo info;
        info.setStage(translate(shader.first));
        if(!shader.second.module)
            shader.second.module = createShaderModule(
                shader.second.binary.get());
        info.setModule(shader.second.module.get());
        info.setPName(shader.second.entry_point.c_str());

        create_infos.push_back(info);
    }
    mPipelineCreateInfo.setStageCount(
        static_cast<uint32_t>(create_infos.size()));
    mPipelineCreateInfo.setPStages(create_infos.data());
}

void usagi::VulkanGraphicsPipelineCompiler::setupVertexInput()
{
    vk::PipelineVertexInputStateCreateInfo create_info;
    create_info.setVertexBindingDescriptionCount(
        static_cast<uint32_t>(mVertexInputBindings.size()));
    create_info.setPVertexBindingDescriptions(
        mVertexInputBindings.data());
    create_info.setVertexAttributeDescriptionCount(
        static_cast<uint32_t>(mVertexAttributeLocationArray.size()));
    create_info.setPVertexAttributeDescriptions(
        mVertexAttributeLocationArray.data());
    mPipelineCreateInfo.setPVertexInputState(&create_info);
}

void usagi::VulkanGraphicsPipelineCompiler::setupDynamicStates()
{
    // Viewport and scissor should be set after binding the pipeline
	std::array<vk::DynamicState, 2> dynamic_state{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	vk::PipelineDynamicStateCreateInfo dynamic_state_create_info;
	dynamic_state_create_info.dynamicStateCount =
        static_cast<uint32_t>(dynamic_state.size());
	dynamic_state_create_info.pDynamicStates = dynamic_state.data();
	mPipelineCreateInfo.pDynamicState = &dynamic_state_create_info;
}

void usagi::VulkanGraphicsPipelineCompiler::setRenderPass(
    std::shared_ptr<RenderPass> render_pass)
{
    mRenderPass = dynamic_pointer_cast_throw<VulkanRenderPass>(render_pass);
    mPipelineCreateInfo.setRenderPass(mRenderPass->renderPass());
}

struct usagi::VulkanGraphicsPipelineCompiler::Context
{
    // Descriptor Set Layouts
    std::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>
        desc_set_layout_bindings;
    std::map<std::uint32_t, vk::UniqueDescriptorSetLayout> desc_set_layouts;
    std::vector<vk::DescriptorSetLayout> desc_set_layout_array;

    // Push Constants
    std::vector<vk::PushConstantRange> push_constants;
    VulkanGraphicsPipeline::PushConstantFieldMap push_constant_field_map;
    // Allocate push constant range linearly for each shader
    std::size_t push_constant_offset = 0;
};

struct usagi::VulkanGraphicsPipelineCompiler::ReflectionHelper
{
    Context &ctx;
    VulkanGraphicsPipelineCompiler *p = nullptr;
    const ShaderMap::value_type &shader;
    const Compiler & compiler;
    ShaderResources resources;
    // total used size of push constant in this shader stage
    std::size_t push_constant_size = 0;
    VulkanGraphicsPipeline::PushConstantFieldMap::value_type::second_type &
        push_constant_fields;

    ReflectionHelper(
        Context &ctx,
        VulkanGraphicsPipelineCompiler *pipeline_compiler,
        const ShaderMap::value_type &shader)
        : ctx { ctx }
        , p { pipeline_compiler }
        , shader { shader }
        , compiler { shader.second.binary->reflectionCompiler() }
        , resources { compiler.get_shader_resources() }
        , push_constant_fields { ctx.push_constant_field_map[shader.first] }
    {
    }

    void reflectFieldPushConstant(const SPIRType &type, unsigned i) const
    {
        const auto &member_name =
            compiler.get_member_name(type.self, i);
        const auto member_offset =
            compiler.type_struct_member_offset(type, i);
        const auto member_size =
            compiler.get_declared_struct_member_size(type, i);

        LOG_F(INFO, "%s: offset=%zu, size=%zu",
            member_name.c_str(), member_offset, member_size);

        VulkanPushConstantField field;
        field.size = static_cast<uint32_t>(member_size);
        field.offset = static_cast<uint32_t>(
            ctx.push_constant_offset + // stage offset
            push_constant_size + // previous buffers
            member_offset // member offset
        );
        // add to reflection record
        // todo: if multiple constant buffers are allowed in the
        // future, member name may not uniquely identify the fields.
        // use struct_name.field_name instead?
        push_constant_fields[member_name] = field;
    }

    std::size_t reflectBufferPushConstants(
        const Resource &resource) const
    {
        const auto &type = compiler.get_type(resource.base_type_id);
        const auto size = compiler.get_declared_struct_size(type);

        const auto member_count = type.member_types.size();
        for(unsigned i = 0; i < member_count; i++)
            reflectFieldPushConstant(type, i);

        return size;
    }

    void reflectPushConstantRanges()
    {
        LOG_SCOPE_F(INFO, "Push constant range allocations (offset=%zu):",
            ctx.push_constant_offset
        );

        for(const auto &resource : resources.push_constant_buffers)
            push_constant_size += reflectBufferPushConstants(resource);

        vk::PushConstantRange range;
        range.setOffset(static_cast<uint32_t>(ctx.push_constant_offset));
        range.setSize(static_cast<uint32_t>(push_constant_size));
        range.setStageFlags(translate(shader.first));
        ctx.push_constants.push_back(range);

        ctx.push_constant_offset += push_constant_size;
    }

    void reflectVertexInputAttributes()
    {
        LOG_SCOPE_F(INFO, "Vertex input attribtues:");

        for(auto &&resource : resources.stage_inputs)
        {
            const auto location = compiler.get_decoration(
                resource.id, spv::DecorationLocation);

            LOG_F(INFO, "%s: location=%u",
                resource.name.c_str(), location);

            // normalize to location-based indexing
            const auto it = p->mVertexAttributeNameMap.find(resource.name);
            if(it != p->mVertexAttributeNameMap.end())
            {
                it->second.location = location;
                p->mVertexAttributeLocationArray.push_back(it->second);
                p->mVertexAttributeNameMap.erase(it);
            }
        }
    }

    void addResource(std::vector<Resource>::value_type &resource,
        vk::DescriptorType resource_type) const
    {
        // todo: ensure different stages uses different set indices
        const auto set = compiler.get_decoration(
            resource.id, spv::DecorationDescriptorSet);
        const auto binding = compiler.get_decoration(
            resource.id, spv::DecorationBinding);
        const auto &type = compiler.get_type(resource.type_id);

        vk::DescriptorSetLayoutBinding layout_binding;
        layout_binding.setStageFlags(translate(shader.first));
        layout_binding.setBinding(binding);
        layout_binding.setDescriptorCount(type.vecsize);
        layout_binding.setDescriptorType(resource_type);

        ctx.desc_set_layout_bindings[set].push_back(layout_binding);
    }

    void reflectDescriptorSets()
    {
        LOG_SCOPE_F(INFO, "Descriptor set layouts:");

        // todo: others resource types

        for(auto &&resource : resources.separate_images)
            addResource(resource, vk::DescriptorType::eSampledImage);

        for(auto &&resource : resources.separate_samplers)
            addResource(resource, vk::DescriptorType::eSampler);

        for(auto &&resource : resources.uniform_buffers)
            addResource(resource, vk::DescriptorType::eUniformBuffer);

        for(auto &&resource : resources.subpass_inputs)
            addResource(resource, vk::DescriptorType::eInputAttachment);
    }

    //void reflectRenderTargets()
    //{
    //    for(const auto &resource : resources.stage_outputs)
    //    {
    //        const auto attachment_index = compiler.get_decoration(
    //            resource.id, spv::DecorationInputAttachmentIndex);


    //    }
    //}
};

std::shared_ptr<usagi::GraphicsPipeline> usagi::VulkanGraphicsPipelineCompiler::
    compile()
{
	LOG_F(INFO, "Compiling graphics pipeline...");

	setupShaderStages();
	setupDynamicStates();

	LOG_SCOPE_F(INFO, "Generating pipeline layout...");

    Context ctx;

	for(auto &&shader : mShaders)
	{
        LOG_SCOPE_F(INFO, "Reflecting %s shader", to_string(shader.first));

        ReflectionHelper helper { ctx, this, shader };
        if(shader.first == ShaderStage::VERTEX)
            helper.reflectVertexInputAttributes();
		//if(shader.first == ShaderStage::FRAGMENT)
  //          helper.reflectRenderTargets();
        helper.reflectPushConstantRanges();
        helper.reflectDescriptorSets();
	}

    vk::UniquePipelineLayout compatible_pipeline_layout;
	{
		vk::PipelineLayoutCreateInfo info;

        for(auto &&layout : ctx.desc_set_layout_bindings)
        {
            vk::DescriptorSetLayoutCreateInfo layout_info;
            layout_info.setBindingCount(
                static_cast<uint32_t>(layout.second.size()));
            layout_info.setPBindings(layout.second.data());
            auto l = mDevice->device().createDescriptorSetLayoutUnique(
				layout_info);
            ctx.desc_set_layout_array.push_back(l.get());
            ctx.desc_set_layouts[layout.first] = std::move(l);
        }
        info.setSetLayoutCount(
            static_cast<uint32_t>(ctx.desc_set_layout_array.size()));
        info.setPSetLayouts(ctx.desc_set_layout_array.data());

        info.setPushConstantRangeCount(
            static_cast<uint32_t>(ctx.push_constants.size()));
        info.setPPushConstantRanges(ctx.push_constants.data());

		compatible_pipeline_layout =
			mDevice->device().createPipelineLayoutUnique(info);
		mPipelineCreateInfo.setLayout(compatible_pipeline_layout.get());
	}

    setupVertexInput();

	auto pipeline = mDevice->device().createGraphicsPipelineUnique(
		{ }, mPipelineCreateInfo);

	return std::make_shared<VulkanGraphicsPipeline>(
        std::move(pipeline),
	    std::move(compatible_pipeline_layout),
        mRenderPass,
	    std::move(ctx.desc_set_layouts),
	    std::move(ctx.push_constant_field_map)
    );
}   

usagi::VulkanGraphicsPipelineCompiler::VulkanGraphicsPipelineCompiler(
    VulkanGpuDevice *device)
    : mDevice { device }
{
    // IA
    mPipelineCreateInfo.setPInputAssemblyState(&mInputAssemblyStateCreateInfo);
    setInputAssemblyState({ });

    // Viewport (set as dynamic state in command lists)
    // todo: multiple viewports
    mViewportStateCreateInfo.setViewportCount(1);
    mViewportStateCreateInfo.setScissorCount(1);
    mPipelineCreateInfo.setPViewportState(&mViewportStateCreateInfo);

    // Rasterization
    mPipelineCreateInfo.setPRasterizationState(&mRasterizationStateCreateInfo);
    setRasterizationState({ });

    // Multi-sampling
    // todo: anti-aliasing
    mMultisampleStateCreateInfo.setRasterizationSamples(
        vk::SampleCountFlagBits::e1);
    mMultisampleStateCreateInfo.setMinSampleShading(1.f);
    mPipelineCreateInfo.setPMultisampleState(&mMultisampleStateCreateInfo);

    // Depth/stencil
    // todo

    // Blending
    // todo: multiple attachments
    mColorBlendStateCreateInfo.setAttachmentCount(1);
    mColorBlendStateCreateInfo.setPAttachments(&mColorBlendAttachmentState);
    mPipelineCreateInfo.setPColorBlendState(&mColorBlendStateCreateInfo);
    setColorBlendState({ });
}

void usagi::VulkanGraphicsPipelineCompiler::setShader(
    ShaderStage stage,
    std::shared_ptr<SpirvBinary> shader)
{
    ShaderInfo info;
    info.binary = std::move(shader);
    mShaders[stage] = std::move(info);
}

void usagi::VulkanGraphicsPipelineCompiler::setVertexBufferBinding(
    std::uint32_t binding_index,
    std::uint32_t stride,
    VertexInputRate input_rate)
{
    vk::VertexInputBindingDescription vulkan_binding;
    vulkan_binding.setBinding(binding_index);
    vulkan_binding.setStride(stride);
    vulkan_binding.setInputRate(translate(input_rate));

    const auto iter = std::find_if(
        mVertexInputBindings.begin(), mVertexInputBindings.end(),
        [=](auto &&item) { return item.binding == binding_index; }
    );
    if(iter != mVertexInputBindings.end())
        *iter = vulkan_binding; // update old info
    else
        mVertexInputBindings.push_back(vulkan_binding);
}

void usagi::VulkanGraphicsPipelineCompiler::setVertexAttribute(
    std::string attr_name,
    std::uint32_t binding_index,
    std::uint32_t offset,
    GpuDataFormat source_format)
{
    vk::VertexInputAttributeDescription vulkan_attr;
    // Deferred to compilation when shader reflection is available.
    // vulkan_attr.setLocation();
    vulkan_attr.setBinding(binding_index);
    vulkan_attr.setOffset(offset);
    vulkan_attr.setFormat(translate(source_format));
    mVertexAttributeNameMap.insert(std::make_pair(attr_name, vulkan_attr));
}

void usagi::VulkanGraphicsPipelineCompiler::setVertexAttribute(
    std::uint32_t attr_location,
    std::uint32_t binding_index,
    std::uint32_t offset,
    GpuDataFormat source_format)
{
    vk::VertexInputAttributeDescription vulkan_attr;
    vulkan_attr.setLocation(attr_location);
    vulkan_attr.setBinding(binding_index);
    vulkan_attr.setOffset(offset);
    vulkan_attr.setFormat(translate(source_format));
    mVertexAttributeLocationArray.push_back(vulkan_attr);
}

void usagi::VulkanGraphicsPipelineCompiler::setInputAssemblyState(
    const InputAssemblyState &state)
{
    mInputAssemblyStateCreateInfo.setTopology(
        translate(state.topology));
}

void usagi::VulkanGraphicsPipelineCompiler::setRasterizationState(
    const RasterizationState &state)
{
    mRasterizationStateCreateInfo.setDepthBiasEnable(false);
    mRasterizationStateCreateInfo.setCullMode(
        translate(state.face_culling_mode));
    mRasterizationStateCreateInfo.setFrontFace(
        translate(state.front_face));
    mRasterizationStateCreateInfo.setPolygonMode(
        translate(state.polygon_mode));
    mRasterizationStateCreateInfo.setLineWidth(1.f);
}

void usagi::VulkanGraphicsPipelineCompiler::setDepthStencilState(
    const DepthStencilState &state)
{
    // todo
}

void usagi::VulkanGraphicsPipelineCompiler::setColorBlendState(
    const ColorBlendState &state)
{
    mColorBlendAttachmentState.setColorBlendOp(
        translate(state.color_blend_op));
    mColorBlendAttachmentState.setAlphaBlendOp(
        translate(state.alpha_blend_op));
    mColorBlendAttachmentState.setSrcColorBlendFactor(
        translate(state.src_color_factor));
    mColorBlendAttachmentState.setDstColorBlendFactor(
        translate(state.dst_color_factor));
    mColorBlendAttachmentState.setSrcAlphaBlendFactor(
        translate(state.src_alpha_factor));
    mColorBlendAttachmentState.setDstAlphaBlendFactor(
        translate(state.dst_alpha_factor));
    mColorBlendAttachmentState.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
}
