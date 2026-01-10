module Render.Renderer2D;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;
import Render.VirtualTextureManager;
import glm;
import <cstddef>;
import "glm/gtx/transform.hpp";

namespace
Engine {
    Renderer2D::Renderer2D(Renderer2DDescriptor desc)
        : mDevice(std::move(desc.Device)), mOutputSize(desc.OutputSize), mVirtualSize(desc.VirtualSize),
          mVirtualTextureManager(mDevice) {
        CreateResources();
        CreateConstantBuffers();
        CreatePipelines();
        CreatePipelineResources();
        RecalculateViewProjectionMatrix();
    }

    void Renderer2D::CreatePipelineResources() {
        CreateTriangleBatchRenderingResources(4);
        // this should be enough for most cases, if not we can always expand it
        CreateLineBatchRenderingResources(4); // same for lines
        CreateEllipseBatchRenderingResources(4); // same for ellipses
    }

    void Renderer2D::BeginRendering() {
        Clear();

        mCommandList->open();

        mCommandList->setResourceStatesForFramebuffer(mFramebuffer);
        mCommandList->clearTextureFloat(mTexture,
                                        nvrhi::AllSubresources, nvrhi::Color(0.f, 0.f, 0.f, 0.f));
    }

    void Renderer2D::EndRendering() {
        Submit();
        mCommandList->close();
        mDevice->executeCommandList(mCommandList);

        if (mVirtualTextureManager.IsSubOptimal()) {
            mVirtualTextureManager.Optimize();
        }
    }

    void Renderer2D::OnResize(uint32_t width, uint32_t height) {
        if (width == mOutputSize.x && height == mOutputSize.y) {
            return;
        }
        mDevice->waitForIdle();

        mOutputSize = glm::u32vec2(width, height);
        mTexture.Reset();
        mFramebuffer.Reset();

        CreateResources();

        RecalculateViewProjectionMatrix();
    }

    void Renderer2D::CreateResources() {
        nvrhi::TextureDesc texDesc;
        texDesc.width = mOutputSize.x;
        texDesc.height = mOutputSize.y;
        texDesc.format = nvrhi::Format::RGBA8_UNORM;
        texDesc.isRenderTarget = true;
        texDesc.isShaderResource = true;
        texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        texDesc.keepInitialState = true;
        texDesc.clearValue = nvrhi::Color(0.f, 0.f, 0.f, 0.f);

        auto tex = mDevice->createTexture(texDesc);
        mTexture = tex;
        mFramebuffer = mDevice->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(tex));

        if (!mCommandList) {
            mCommandList = mDevice->createCommandList();
        }

        mTextureSampler = mDevice->createSampler(nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp)
            .setAllFilters(true));

        vk::PhysicalDevice vkPhysicalDevice = static_cast<vk::PhysicalDevice>(
            mDevice->getNativeObject(nvrhi::ObjectTypes::VK_PhysicalDevice)
        );

        vk::PhysicalDeviceProperties deviceProperties = vkPhysicalDevice.getProperties();

        uint32_t hardwareMax = deviceProperties.limits.maxDescriptorSetSampledImages;

        mBindlessTextureArraySizeMax = std::min<uint32_t>(16384u, hardwareMax);
        mTriangleBufferInstanceSizeMax = 1 << 18; // 2^18 instances
        mLineBufferVertexSizeMax = 1 << 18; // 2^18 vertices
        mEllipseBufferInstanceSizeMax = 1 << 16; // 2^16 ellipses (each ellipse = 6 vertices)
    }

    void Renderer2D::CreateTriangleBatchRenderingResources(size_t count) {
        if (count <= mTriangleBatchRenderingResources.size()) {
            return;
        }

        for (size_t i = mTriangleBatchRenderingResources.size(); i < count; ++i) {
            TriangleBatchRenderingResources resources;

            nvrhi::BufferDesc vertexBufferDesc;
            vertexBufferDesc.byteSize = sizeof(TriangleVertexData) * mTriangleBufferInstanceSizeMax * 4;
            vertexBufferDesc.isVertexBuffer = true;
            vertexBufferDesc.debugName = "Renderer2D::TriangleVertexBuffer";
            vertexBufferDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
            vertexBufferDesc.keepInitialState = true;
            resources.VertexBuffer = mDevice->createBuffer(vertexBufferDesc);

            nvrhi::BufferDesc indexBufferDesc;
            indexBufferDesc.byteSize = sizeof(uint32_t) * mTriangleBufferInstanceSizeMax * 6;
            indexBufferDesc.isIndexBuffer = true;
            indexBufferDesc.debugName = "Renderer2D::TriangleIndexBuffer";
            indexBufferDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
            indexBufferDesc.keepInitialState = true;
            resources.IndexBuffer = mDevice->createBuffer(indexBufferDesc);

            nvrhi::BufferDesc instanceBufferDesc;
            instanceBufferDesc.byteSize = sizeof(TriangleInstanceData) * mTriangleBufferInstanceSizeMax;
            instanceBufferDesc.canHaveRawViews = true;
            instanceBufferDesc.structStride = sizeof(TriangleInstanceData);
            instanceBufferDesc.debugName = "Renderer2D::TriangleInstanceBuffer";
            instanceBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            instanceBufferDesc.keepInitialState = true;
            resources.InstanceBuffer = mDevice->createBuffer(instanceBufferDesc);

            nvrhi::BufferDesc clipBufferDesc;
            clipBufferDesc.byteSize = sizeof(ClipRegion) * mTriangleBufferInstanceSizeMax;
            clipBufferDesc.canHaveRawViews = true;
            clipBufferDesc.structStride = sizeof(ClipRegion);
            clipBufferDesc.debugName = "Renderer2D::TriangleClipBuffer";
            clipBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            clipBufferDesc.keepInitialState = true;
            resources.ClipBuffer = mDevice->createBuffer(clipBufferDesc);

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mTriangleConstantBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, resources.InstanceBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, resources.ClipBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, mTextureSampler));
            resources.mBindingSetSpace0 = mDevice->createBindingSet(bindingSetDesc, mTriangleBindingLayoutSpace0);

            mTriangleBatchRenderingResources.push_back(resources);
        }
    }

    void Renderer2D::CreateLineBatchRenderingResources(size_t count) {
        if (count <= mLineBatchRenderingResources.size()) {
            return;
        }

        for (size_t i = mLineBatchRenderingResources.size(); i < count; ++i) {
            LineBatchRenderingResources resources;

            nvrhi::BufferDesc vertexBufferDesc;
            vertexBufferDesc.byteSize = sizeof(LineVertexData) * mLineBufferVertexSizeMax;
            vertexBufferDesc.isVertexBuffer = true;
            vertexBufferDesc.debugName = "Renderer2D::LineVertexBuffer";
            vertexBufferDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
            vertexBufferDesc.keepInitialState = true;
            resources.VertexBuffer = mDevice->createBuffer(vertexBufferDesc);

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mLineConstantBuffer));
            resources.mBindingSetSpace0 = mDevice->createBindingSet(bindingSetDesc, mLineBindingLayoutSpace0);

            mLineBatchRenderingResources.push_back(resources);
        }
    }

    void Renderer2D::CreateEllipseBatchRenderingResources(size_t count) {
        if (count <= mEllipseBatchRenderingResources.size()) {
            return;
        }

        for (size_t i = mEllipseBatchRenderingResources.size(); i < count; ++i) {
            EllipseBatchRenderingResources resources;

            nvrhi::BufferDesc shapeBufferDesc;
            shapeBufferDesc.byteSize = sizeof(EllipseShapeData) * mEllipseBufferInstanceSizeMax;
            shapeBufferDesc.canHaveRawViews = true;
            shapeBufferDesc.structStride = sizeof(EllipseShapeData);
            shapeBufferDesc.debugName = "Renderer2D::EllipseShapeBuffer";
            shapeBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            shapeBufferDesc.keepInitialState = true;
            resources.ShapeBuffer = mDevice->createBuffer(shapeBufferDesc);

            nvrhi::BufferDesc clipBufferDesc;
            clipBufferDesc.byteSize = sizeof(ClipRegion) * mEllipseBufferInstanceSizeMax;
            clipBufferDesc.canHaveRawViews = true;
            clipBufferDesc.structStride = sizeof(ClipRegion);
            clipBufferDesc.debugName = "Renderer2D::EllipseClipBuffer";
            clipBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
            clipBufferDesc.keepInitialState = true;
            resources.ClipBuffer = mDevice->createBuffer(clipBufferDesc);

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mEllipseConstantBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, resources.ShapeBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, resources.ClipBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, mTextureSampler));
            resources.mBindingSetSpace0 = mDevice->createBindingSet(bindingSetDesc, mEllipseBindingLayoutSpace0);

            mEllipseBatchRenderingResources.push_back(resources);
        }
    }

    void Renderer2D::CreatePipelines() {
        CreatePipelineTriangle();
        CreatePipelineLine();
        CreatePipelineEllipse();
    }

    void Renderer2D::CreateConstantBuffers() {
        nvrhi::BufferDesc constBufferVPMatrixDesc;
        constBufferVPMatrixDesc.byteSize = sizeof(glm::mat4);
        constBufferVPMatrixDesc.isConstantBuffer = true;
        constBufferVPMatrixDesc.debugName = "Renderer2D::ConstantBufferVPMatrix";
        constBufferVPMatrixDesc.initialState = nvrhi::ResourceStates::ShaderResource |
                                               nvrhi::ResourceStates::ConstantBuffer;
        constBufferVPMatrixDesc.keepInitialState = true;
        mTriangleConstantBuffer = mDevice->createBuffer(constBufferVPMatrixDesc);

        nvrhi::BufferDesc constBufferLineDesc;
        constBufferLineDesc.byteSize = sizeof(glm::mat4);
        constBufferLineDesc.isConstantBuffer = true;
        constBufferLineDesc.debugName = "Renderer2D::LineConstantBufferVPMatrix";
        constBufferLineDesc.initialState = nvrhi::ResourceStates::ShaderResource |
                                           nvrhi::ResourceStates::ConstantBuffer;
        constBufferLineDesc.keepInitialState = true;
        mLineConstantBuffer = mDevice->createBuffer(constBufferLineDesc);

        nvrhi::BufferDesc constBufferEllipseDesc;
        constBufferEllipseDesc.byteSize = sizeof(glm::mat4);
        constBufferEllipseDesc.isConstantBuffer = true;
        constBufferEllipseDesc.debugName = "Renderer2D::EllipseConstantBufferVPMatrix";
        constBufferEllipseDesc.initialState = nvrhi::ResourceStates::ShaderResource |
                                              nvrhi::ResourceStates::ConstantBuffer;
        constBufferEllipseDesc.keepInitialState = true;
        mEllipseConstantBuffer = mDevice->createBuffer(constBufferEllipseDesc);
    }

    void Renderer2D::CreatePipelineTriangle() {
        nvrhi::ShaderDesc vsDesc;
        vsDesc.shaderType = nvrhi::ShaderType::Vertex;
        vsDesc.entryName = "main";
        nvrhi::ShaderHandle vs = mDevice->createShader(vsDesc,
                                                       GeneratedShaders::renderer2d_triangle_vs.data(),
                                                       GeneratedShaders::renderer2d_triangle_vs.size());

        nvrhi::ShaderDesc psDesc;
        psDesc.shaderType = nvrhi::ShaderType::Pixel;
        psDesc.entryName = "main";
        nvrhi::ShaderHandle ps = mDevice->createShader(psDesc,
                                                       GeneratedShaders::renderer2d_triangle_ps.data(),
                                                       GeneratedShaders::renderer2d_triangle_ps.size());

        nvrhi::VertexAttributeDesc posAttrs[3];
        posAttrs[0].name = "POSITION";
        posAttrs[0].format = nvrhi::Format::RG32_FLOAT;
        posAttrs[0].bufferIndex = 0;
        posAttrs[0].offset = offsetof(TriangleVertexData, Position);
        posAttrs[0].elementStride = sizeof(TriangleVertexData);

        posAttrs[1].name = "TEXCOORD";
        posAttrs[1].format = nvrhi::Format::RG32_FLOAT;
        posAttrs[1].bufferIndex = 0;
        posAttrs[1].offset = offsetof(TriangleVertexData, TexCoords);
        posAttrs[1].elementStride = sizeof(TriangleVertexData);

        posAttrs[2].name = "CONSTANTINDEX";
        posAttrs[2].format = nvrhi::Format::R32_UINT;
        posAttrs[2].bufferIndex = 0;
        posAttrs[2].offset = offsetof(TriangleVertexData, InstanceIndex);
        posAttrs[2].elementStride = sizeof(TriangleVertexData);

        mTriangleInputLayout = mDevice->createInputLayout(posAttrs, 3, vs);

        nvrhi::BindingLayoutDesc bindingLayoutDesc[2];
        bindingLayoutDesc[0].visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;
        bindingLayoutDesc[0].bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(0),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
            nvrhi::BindingLayoutItem::Sampler(0)
        };

        bindingLayoutDesc[1].visibility = nvrhi::ShaderType::Pixel;
        bindingLayoutDesc[1].bindings = {
            nvrhi::BindingLayoutItem::Texture_SRV(0).setSize(mBindlessTextureArraySizeMax)
        };

        mTriangleBindingLayoutSpace0 = mDevice->createBindingLayout(bindingLayoutDesc[0]);
        mTriangleBindingLayoutSpace1 = mDevice->createBindingLayout(bindingLayoutDesc[1]);

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.VS = vs;
        pipeDesc.PS = ps;
        pipeDesc.inputLayout = mTriangleInputLayout;
        pipeDesc.bindingLayouts = {
            mTriangleBindingLayoutSpace0,
            mTriangleBindingLayoutSpace1
        };

        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;

        pipeDesc.renderState.blendState.targets[0].blendEnable = true;
        pipeDesc.renderState.blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        pipeDesc.renderState.blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].colorWriteMask = nvrhi::ColorMask::All;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;

        mTrianglePipeline = mDevice->createGraphicsPipeline(pipeDesc, mFramebuffer->getFramebufferInfo());
    }

    void Renderer2D::CreatePipelineLine() {
        nvrhi::ShaderDesc vsDesc;
        vsDesc.shaderType = nvrhi::ShaderType::Vertex;
        vsDesc.entryName = "main";
        nvrhi::ShaderHandle vs = mDevice->createShader(vsDesc,
                                                       GeneratedShaders::renderer2d_line_vs.data(),
                                                       GeneratedShaders::renderer2d_line_vs.size());

        nvrhi::ShaderDesc psDesc;
        psDesc.shaderType = nvrhi::ShaderType::Pixel;
        psDesc.entryName = "main";
        nvrhi::ShaderHandle ps = mDevice->createShader(psDesc,
                                                       GeneratedShaders::renderer2d_line_ps.data(),
                                                       GeneratedShaders::renderer2d_line_ps.size());

        nvrhi::VertexAttributeDesc posAttrs[2];
        posAttrs[0].name = "POSITION";
        posAttrs[0].format = nvrhi::Format::RG32_FLOAT;
        posAttrs[0].bufferIndex = 0;
        posAttrs[0].offset = offsetof(LineVertexData, Position);
        posAttrs[0].elementStride = sizeof(LineVertexData);

        posAttrs[1].name = "COLOR";
        posAttrs[1].format = nvrhi::Format::R32_UINT;
        posAttrs[1].bufferIndex = 0;
        posAttrs[1].offset = offsetof(LineVertexData, Color);
        posAttrs[1].elementStride = sizeof(LineVertexData);

        mLineInputLayout = mDevice->createInputLayout(posAttrs, 2, vs);

        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.visibility = nvrhi::ShaderType::Vertex;
        bindingLayoutDesc.bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(0)
        };

        mLineBindingLayoutSpace0 = mDevice->createBindingLayout(bindingLayoutDesc);

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.VS = vs;
        pipeDesc.PS = ps;
        pipeDesc.inputLayout = mLineInputLayout;
        pipeDesc.bindingLayouts = {
            mLineBindingLayoutSpace0
        };

        pipeDesc.primType = nvrhi::PrimitiveType::LineList;

        pipeDesc.renderState.blendState.targets[0].blendEnable = true;
        pipeDesc.renderState.blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        pipeDesc.renderState.blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].colorWriteMask = nvrhi::ColorMask::All;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;

        mLinePipeline = mDevice->createGraphicsPipeline(pipeDesc, mFramebuffer->getFramebufferInfo());
    }

    void Renderer2D::CreatePipelineEllipse() {
        nvrhi::ShaderDesc vsDesc;
        vsDesc.shaderType = nvrhi::ShaderType::Vertex;
        vsDesc.entryName = "main";
        nvrhi::ShaderHandle vs = mDevice->createShader(vsDesc,
                                                       GeneratedShaders::renderer2d_ellipse_vs.data(),
                                                       GeneratedShaders::renderer2d_ellipse_vs.size());

        nvrhi::ShaderDesc psDesc;
        psDesc.shaderType = nvrhi::ShaderType::Pixel;
        psDesc.entryName = "main";
        nvrhi::ShaderHandle ps = mDevice->createShader(psDesc,
                                                       GeneratedShaders::renderer2d_ellipse_ps.data(),
                                                       GeneratedShaders::renderer2d_ellipse_ps.size());

        nvrhi::BindingLayoutDesc bindingLayoutDesc[2];
        bindingLayoutDesc[0].visibility = nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel;
        bindingLayoutDesc[0].bindings = {
            nvrhi::BindingLayoutItem::ConstantBuffer(0),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
            nvrhi::BindingLayoutItem::Sampler(0)
        };

        bindingLayoutDesc[1].visibility = nvrhi::ShaderType::Pixel;
        bindingLayoutDesc[1].bindings = {
            nvrhi::BindingLayoutItem::Texture_SRV(0).setSize(mBindlessTextureArraySizeMax)
        };

        mEllipseBindingLayoutSpace0 = mDevice->createBindingLayout(bindingLayoutDesc[0]);
        mEllipseBindingLayoutSpace1 = mDevice->createBindingLayout(bindingLayoutDesc[1]);

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.VS = vs;
        pipeDesc.PS = ps;
        pipeDesc.bindingLayouts = {
            mEllipseBindingLayoutSpace0,
            mEllipseBindingLayoutSpace1
        };

        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;

        pipeDesc.renderState.blendState.targets[0].blendEnable = true;
        pipeDesc.renderState.blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        pipeDesc.renderState.blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].colorWriteMask = nvrhi::ColorMask::All;
        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;

        mEllipsePipeline = mDevice->createGraphicsPipeline(pipeDesc, mFramebuffer->getFramebufferInfo());
    }

    void Renderer2D::SubmitTriangleBatchRendering() {
        auto submissions = mTriangleCommandList.RecordRendererSubmissionData(
            mTriangleBufferInstanceSizeMax);

        CreateTriangleBatchRenderingResources(submissions.size());

        // submit constant buffer
        mCommandList->writeBuffer(mTriangleConstantBuffer, &mViewProjectionMatrix,
                                  sizeof(glm::mat4), 0);

        for (size_t i = 0; i < submissions.size(); ++i) {
            auto &submission = submissions[i];
            auto &resources = mTriangleBatchRenderingResources[i];

            // Update Buffers
            if (!submission.VertexData.empty()) {
                mCommandList->writeBuffer(resources.VertexBuffer, submission.VertexData.data(),
                                          sizeof(TriangleVertexData) * submission.VertexData.size(), 0);
            }

            if (!submission.IndexData.empty()) {
                mCommandList->writeBuffer(resources.IndexBuffer, submission.IndexData.data(),
                                          sizeof(uint32_t) * submission.IndexData.size(), 0);
            }

            if (!submission.InstanceData.empty()) {
                mCommandList->writeBuffer(resources.InstanceBuffer, submission.InstanceData.data(),
                                          sizeof(TriangleInstanceData) * submission.InstanceData.size(), 0);
            }

            if (!submission.ClipData.empty()) {
                mCommandList->writeBuffer(resources.ClipBuffer, submission.ClipData.data(),
                                          sizeof(ClipRegion) * submission.ClipData.size(), 0);
            }


            mCommandList->setResourceStatesForBindingSet(resources.mBindingSetSpace0);
            auto bindingSetSpace1 = mVirtualTextureManager.GetBindingSet(mTriangleBindingLayoutSpace1);
            mCommandList->setResourceStatesForBindingSet(bindingSetSpace1);

            // Draw Call
            nvrhi::GraphicsState state;
            state.pipeline = mTrianglePipeline;
            state.framebuffer = mFramebuffer;
            state.viewport.addViewportAndScissorRect(
                mFramebuffer->getFramebufferInfo().getViewport());
            state.bindings.push_back(resources.mBindingSetSpace0);
            state.bindings.push_back(bindingSetSpace1);

            nvrhi::VertexBufferBinding vertexBufferBinding;
            vertexBufferBinding.buffer = resources.VertexBuffer;
            vertexBufferBinding.offset = 0;
            vertexBufferBinding.slot = 0;

            state.vertexBuffers.push_back(vertexBufferBinding);

            nvrhi::IndexBufferBinding indexBufferBinding;
            indexBufferBinding.buffer = resources.IndexBuffer;
            indexBufferBinding.format = nvrhi::Format::R32_UINT;
            indexBufferBinding.offset = 0;

            state.indexBuffer = indexBufferBinding;

            mCommandList->setGraphicsState(state);

            nvrhi::DrawArguments drawArgs;
            drawArgs.vertexCount = static_cast<uint32_t>(submission.IndexData.size());

            mCommandList->drawIndexed(drawArgs);
        }

        mTriangleCommandList.GiveBackForNextFrame(std::move(submissions));
    }

    void Renderer2D::SubmitLineBatchRendering() {
        auto submissions = mLineCommandList.RecordRendererSubmissionData(
            mLineBufferVertexSizeMax);

        if (submissions.empty()) {
            return;
        }

        CreateLineBatchRenderingResources(submissions.size());

        // submit constant buffer
        mCommandList->writeBuffer(mLineConstantBuffer, &mViewProjectionMatrix,
                                  sizeof(glm::mat4), 0);

        for (size_t i = 0; i < submissions.size(); ++i) {
            auto &submission = submissions[i];
            auto &resources = mLineBatchRenderingResources[i];

            // Update Buffers
            if (!submission.VertexData.empty()) {
                mCommandList->writeBuffer(resources.VertexBuffer, submission.VertexData.data(),
                                          sizeof(LineVertexData) * submission.VertexData.size(), 0);
            } else {
                continue;
            }

            mCommandList->setResourceStatesForBindingSet(resources.mBindingSetSpace0);

            // Draw Call
            nvrhi::GraphicsState state;
            state.pipeline = mLinePipeline;
            state.framebuffer = mFramebuffer;
            state.viewport.addViewportAndScissorRect(
                mFramebuffer->getFramebufferInfo().getViewport());
            state.bindings.push_back(resources.mBindingSetSpace0);

            nvrhi::VertexBufferBinding vertexBufferBinding;
            vertexBufferBinding.buffer = resources.VertexBuffer;
            vertexBufferBinding.offset = 0;
            vertexBufferBinding.slot = 0;

            state.vertexBuffers.push_back(vertexBufferBinding);

            mCommandList->setGraphicsState(state);

            nvrhi::DrawArguments drawArgs;
            drawArgs.vertexCount = static_cast<uint32_t>(submission.VertexData.size());

            mCommandList->draw(drawArgs);
        }

        mLineCommandList.GiveBackForNextFrame(std::move(submissions));
    }

    void Renderer2D::SubmitEllipseBatchRendering() {
        auto submissions = mEllipseCommandList.RecordRendererSubmissionData(
            mEllipseBufferInstanceSizeMax);

        if (submissions.empty()) {
            return;
        }

        CreateEllipseBatchRenderingResources(submissions.size());

        mCommandList->writeBuffer(mEllipseConstantBuffer, &mViewProjectionMatrix,
                                  sizeof(glm::mat4), 0);

        for (size_t i = 0; i < submissions.size(); ++i) {
            auto &submission = submissions[i];
            auto &resources = mEllipseBatchRenderingResources[i];

            if (submission.ShapeData.empty()) {
                continue;
            }

            mCommandList->writeBuffer(resources.ShapeBuffer, submission.ShapeData.data(),
                                      sizeof(EllipseShapeData) * submission.ShapeData.size(), 0);

            if (!submission.ClipData.empty()) {
                mCommandList->writeBuffer(resources.ClipBuffer, submission.ClipData.data(),
                                          sizeof(ClipRegion) * submission.ClipData.size(), 0);
            }

            mCommandList->setResourceStatesForBindingSet(resources.mBindingSetSpace0);
            auto bindingSetSpace1 = mVirtualTextureManager.GetBindingSet(mEllipseBindingLayoutSpace1);
            mCommandList->setResourceStatesForBindingSet(bindingSetSpace1);

            nvrhi::GraphicsState state;
            state.pipeline = mEllipsePipeline;
            state.framebuffer = mFramebuffer;
            state.viewport.addViewportAndScissorRect(
                mFramebuffer->getFramebufferInfo().getViewport());
            state.bindings.push_back(resources.mBindingSetSpace0);
            state.bindings.push_back(bindingSetSpace1);

            mCommandList->setGraphicsState(state);

            nvrhi::DrawArguments drawArgs;
            drawArgs.vertexCount = static_cast<uint32_t>(submission.ShapeData.size() * 6);

            mCommandList->draw(drawArgs);
        }

        mEllipseCommandList.GiveBackForNextFrame(std::move(submissions));
    }

    void Renderer2D::Submit() {
        SubmitTriangleBatchRendering();
        SubmitLineBatchRendering();
        SubmitEllipseBatchRendering();
    }

    void Renderer2D::RecalculateViewProjectionMatrix() {
        float scaleX = static_cast<float>(mOutputSize.x) / mVirtualSize.x;
        float scaleY = static_cast<float>(mOutputSize.y) / mVirtualSize.y;

        float uniformScale = std::min(scaleX, scaleY);

        float halfVisibleWidth = static_cast<float>(mOutputSize.x) / (2.0f * uniformScale);
        float halfVisibleHeight = static_cast<float>(mOutputSize.y) / (2.0f * uniformScale);

        mViewProjectionMatrix = glm::ortho(
            -halfVisibleWidth, // Left
            halfVisibleWidth, // Right
            halfVisibleHeight, // Bottom
            -halfVisibleHeight, // Top
            -1.0f, // zNear
            1.0f // zFar
        );
    }

    nvrhi::ITexture *Renderer2D::GetTexture() const {
        return mTexture.Get();
    }

    void Renderer2D::Clear() {
        mTriangleCommandList.Clear();
        mLineCommandList.Clear();
        mEllipseCommandList.Clear();
    }

    uint32_t Renderer2D::RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle &texture) {
        return mVirtualTextureManager.RegisterTexture(texture);
    }

    void Renderer2D::DrawTriangleColored(const glm::mat3x2 &positions,
                                         const glm::u8vec4 &color,
                                         std::optional<int> overrideDepth,
                                         const ClipRegion *clip) {
        mTriangleCommandList.AddTriangle(
            positions[0], glm::vec2(0.f, 0.f),
            positions[1], glm::vec2(0.f, 0.f),
            positions[2], glm::vec2(0.f, 0.f),
            -1,
            (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
    }

    void Renderer2D::DrawTriangleTextureVirtual(const glm::mat3x2 &positions,
                                                const glm::mat3x2 &uvs,
                                                uint32_t virtualTextureID,
                                                std::optional<int> overrideDepth,
                                                glm::u8vec4 tintColor, const ClipRegion *clip) {
        mTriangleCommandList.AddTriangle(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            static_cast<int>(virtualTextureID),
            (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
    }

    uint32_t Renderer2D::DrawTriangleTextureManaged(const glm::mat3x2 &positions,
                                                    const glm::mat3x2 &uvs,
                                                    const nvrhi::TextureHandle &texture,
                                                    std::optional<int> overrideDepth,
                                                    glm::u8vec4 tintColor, const ClipRegion *clip) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        mTriangleCommandList.AddTriangle(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            static_cast<int>(virtualTextureID),
            (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
        return virtualTextureID;
    }

    void Renderer2D::DrawQuadColored(const glm::mat4x2 &positions,
                                     const glm::u8vec4 &color,
                                     std::optional<int> overrideDepth, const ClipRegion *clip) {
        mTriangleCommandList.AddQuad(
            positions[0], glm::vec2(0.f, 0.f),
            positions[1], glm::vec2(0.f, 0.f),
            positions[2], glm::vec2(0.f, 0.f),
            positions[3], glm::vec2(0.f, 0.f),
            -1,
            (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
    }

    void Renderer2D::DrawQuadTextureVirtual(const glm::mat4x2 &positions,
                                            const glm::mat4x2 &uvs,
                                            uint32_t virtualTextureID,
                                            std::optional<int> overrideDepth,
                                            glm::u8vec4 tintColor, const ClipRegion *clip) {
        mTriangleCommandList.AddQuad(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            positions[3], uvs[3],
            static_cast<int>(virtualTextureID),
            (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
    }

    uint32_t Renderer2D::DrawQuadTextureManaged(const glm::mat4x2 &positions,
                                                const glm::mat4x2 &uvs,
                                                const nvrhi::TextureHandle &texture,
                                                std::optional<int> overrideDepth,
                                                glm::u8vec4 tintColor, const ClipRegion *clip) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        mTriangleCommandList.AddQuad(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            positions[3], uvs[3],
            static_cast<int>(virtualTextureID),
            (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clip);
        return virtualTextureID;
    }

    void Renderer2D::DrawLine(const glm::vec2 &p0, const glm::vec2 &p1,
                              const glm::u8vec4 &color) {
        mLineCommandList.AddLine(p0, color, p1, color);
    }

    void Renderer2D::DrawLine(const glm::vec2 &p0, const glm::vec2 &p1,
                              const glm::u8vec4 &color0, const glm::u8vec4 &color1) {
        mLineCommandList.AddLine(p0, color0, p1, color1);
    }

    void Renderer2D::DrawCircle(const glm::vec2 &center, float radius,
                                const glm::u8vec4 &color,
                                std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Circle(
            center, radius, color, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipse(const glm::vec2 &center, const glm::vec2 &radii,
                                 float rotation, const glm::u8vec4 &color,
                                 std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Ellipse(
            center, radii, rotation, color, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawRing(const glm::vec2 &center, float outerRadius, float innerRadius,
                              const glm::u8vec4 &color,
                              std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Ring(
            center, outerRadius, innerRadius, color, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawSector(const glm::vec2 &center, float radius,
                                float startAngle, float endAngle,
                                const glm::u8vec4 &color,
                                std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, color, -1, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawSectorTextureVirtual(const glm::vec2 &center, float radius,
                                              float startAngle, float endAngle,
                                              uint32_t virtualTextureID,
                                              const glm::u8vec4 &tintColor,
                                              std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawSectorTextureManaged(const glm::vec2 &center, float radius,
                                                  float startAngle, float endAngle,
                                                  const nvrhi::TextureHandle &texture,
                                                  const glm::u8vec4 &tintColor,
                                                  std::optional<int> overrideDepth, const ClipRegion *clip) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
        return virtualTextureID;
    }

    void Renderer2D::DrawArc(const glm::vec2 &center, float radius, float thickness,
                             float startAngle, float endAngle,
                             const glm::u8vec4 &color,
                             std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::Arc(
            center, radius, thickness, startAngle, endAngle, color, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                       float rotation, float startAngle, float endAngle,
                                       const glm::u8vec4 &color,
                                       std::optional<int> overrideDepth, const ClipRegion *clip) {
        EllipseRenderingData data = EllipseRenderingData::EllipseSector(
            center, radii, rotation, startAngle, endAngle, color, -1, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseSectorTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii,
                                                     float rotation, float startAngle, float endAngle,
                                                     uint32_t virtualTextureID,
                                                     const glm::u8vec4 &tintColor,
                                                     std::optional<int> overrideDepth,const ClipRegion* clip) {
        EllipseRenderingData data = EllipseRenderingData::EllipseSector(
            center, radii, rotation, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                    float rotation, float thickness,
                                    float startAngle, float endAngle,
                                    const glm::u8vec4 &color,
                                    std::optional<int> overrideDepth, const ClipRegion* clip) {
        EllipseRenderingData data = EllipseRenderingData::EllipseArc(
            center, radii, rotation, thickness, startAngle, endAngle, color, overrideDepth.value_or(mCurrentDepth), clip);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawCircleTextureVirtual(const glm::vec2 &center, float radius,
                                              uint32_t virtualTextureID,
                                              const glm::u8vec4 &tintColor,
                                              std::optional<int> overrideDepth, const ClipRegion* clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.VirtualTextureID = static_cast<int>(virtualTextureID);
        data.TintColor = (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a;
        data.Depth = overrideDepth.value_or(mCurrentDepth);
        data.Clip = clip ? std::optional{*clip} : std::nullopt;
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawCircleTextureManaged(const glm::vec2 &center, float radius,
                                                  const nvrhi::TextureHandle &texture,
                                                  const glm::u8vec4 &tintColor,
                                                  std::optional<int> overrideDepth, const ClipRegion* clip) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        DrawCircleTextureVirtual(center, radius, virtualTextureID, tintColor, overrideDepth, clip);
        return virtualTextureID;
    }

    void Renderer2D::DrawEllipseTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii,
                                               float rotation, uint32_t virtualTextureID,
                                               const glm::u8vec4 &tintColor,
                                               std::optional<int> overrideDepth, const ClipRegion* clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.VirtualTextureID = static_cast<int>(virtualTextureID);
        data.TintColor = (tintColor.r << 24) | (tintColor.g << 16) | (tintColor.b << 8) | tintColor.a;
        data.Depth = overrideDepth.value_or(mCurrentDepth);
        data.Clip = clip ? std::optional{*clip} : std::nullopt;
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawEllipseTextureManaged(const glm::vec2 &center, const glm::vec2 &radii,
                                                   float rotation,
                                                   const nvrhi::TextureHandle &texture,
                                                   const glm::u8vec4 &tintColor,
                                                   std::optional<int> overrideDepth, const ClipRegion* clip) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        DrawEllipseTextureVirtual(center, radii, rotation, virtualTextureID, tintColor, overrideDepth, clip);
        return virtualTextureID;
    }
}

#pragma region implementation

namespace
Engine {
    TriangleRenderingData TriangleRenderingData::Triangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                          const glm::vec2 &p1, const glm::vec2 &uv1,
                                                          const glm::vec2 &p2, const glm::vec2 &uv2,
                                                          int textureIndex,
                                                          uint32_t tintColor, int depth,
                                                          const ClipRegion *clip) {
        TriangleRenderingData data;
        data.Positions[0] = p0;
        data.Positions[1] = p1;
        data.Positions[2] = p2;
        data.TexCoords[0] = uv0;
        data.TexCoords[1] = uv1;
        data.TexCoords[2] = uv2;
        data.IsQuad = false;
        data.VirtualTextureID = textureIndex;
        data.TintColor = tintColor;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    TriangleRenderingData TriangleRenderingData::Quad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                      const glm::vec2 &p1, const glm::vec2 &uv1,
                                                      const glm::vec2 &p2, const glm::vec2 &uv2,
                                                      const glm::vec2 &p3, const glm::vec2 &uv3,
                                                      int virtualTextureID,
                                                      uint32_t tintColor, int depth,
                                                      const ClipRegion *clip) {
        TriangleRenderingData data;
        data.Positions[0] = p0;
        data.Positions[1] = p1;
        data.Positions[2] = p2;
        data.Positions[3] = p3;
        data.TexCoords[0] = uv0;
        data.TexCoords[1] = uv1;
        data.TexCoords[2] = uv2;
        data.TexCoords[3] = uv3;
        data.IsQuad = true;
        data.VirtualTextureID = virtualTextureID;
        data.TintColor = tintColor;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    void TriangleRenderingSubmissionData::Clear() {
        VertexData.clear();
        IndexData.clear();
        InstanceData.clear();
        ClipData.clear();
    }

    void TriangleRenderingCommandList::AddTriangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                   const glm::vec2 &p1, const glm::vec2 &uv1,
                                                   const glm::vec2 &p2, const glm::vec2 &uv2,
                                                   int virtualTextureID,
                                                   uint32_t tintColor,
                                                   int depth, const ClipRegion *clip) {
        Instances.resize(Instances.size() + 1);
        Instances.back() = TriangleRenderingData::Triangle(
            p0, uv0, p1, uv1, p2, uv2, virtualTextureID, tintColor, depth, clip);
    }

    void TriangleRenderingCommandList::AddQuad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                               const glm::vec2 &p1, const glm::vec2 &uv1,
                                               const glm::vec2 &p2, const glm::vec2 &uv2,
                                               const glm::vec2 &p3, const glm::vec2 &uv3,
                                               int virtualTextureID,
                                               uint32_t tintColor,
                                               int depth, const ClipRegion *clip) {
        Instances.resize(Instances.size() + 1);
        Instances.back() = TriangleRenderingData::Quad(
            p0, uv0, p1, uv1, p2, uv2, p3, uv3, virtualTextureID, tintColor, depth, clip);
    }

    void TriangleRenderingCommandList::Clear() {
        Instances.clear();
    }

    std::vector<TriangleRenderingSubmissionData> TriangleRenderingCommandList::RecordRendererSubmissionData(
        size_t triangleBufferInstanceSizeMax) {
        std::ranges::sort(Instances, [](const auto &a, const auto &b) {
            if (a.Depth != b.Depth) return a.Depth < b.Depth;
            return a.VirtualTextureID < b.VirtualTextureID;
        });

        std::vector<TriangleRenderingSubmissionData> submissions;
        if (Instances.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        TriangleRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.VertexData.clear();
            currentSubmission.IndexData.clear();
            currentSubmission.InstanceData.clear();
            currentSubmission.ClipData.clear();
            ++lastFrameSubmissionIt;
        }

        auto finalizeSubmission = [&]() mutable {
            if (!currentSubmission.VertexData.empty()) {
                submissions.push_back(std::move(currentSubmission));

                if (lastFrameSubmissionIt == mLastFrameCache.end()) {
                    currentSubmission.Clear();
                } else {
                    currentSubmission = std::move(*lastFrameSubmissionIt);
                    currentSubmission.VertexData.clear();
                    currentSubmission.IndexData.clear();
                    currentSubmission.InstanceData.clear();
                    currentSubmission.ClipData.clear();
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &instance: Instances) {
            // check if we need to finalize due to vertex/index buffer size
            if (currentSubmission.InstanceData.size() + 1 >
                triangleBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            int32_t finalTextureIndex = instance.VirtualTextureID;

            // Handle clip region
            int32_t clipIndex = -1;
            if (instance.Clip.has_value()) {
                clipIndex = static_cast<int32_t>(currentSubmission.ClipData.size());
                currentSubmission.ClipData.push_back(instance.Clip.value());
            }

            // Fill Instance Data
            auto instanceIndex = static_cast<uint32_t>(currentSubmission.InstanceData.size());

            currentSubmission.InstanceData.reserve(currentSubmission.InstanceData.size());

            currentSubmission.InstanceData.push_back({
                .TintColor = instance.TintColor,
                .TextureIndex = finalTextureIndex,
                .ClipIndex = clipIndex
            });

            uint32_t baseVtx = static_cast<uint32_t>(currentSubmission.VertexData.size());

            if (!instance.IsQuad) {
                currentSubmission.VertexData.resize(currentSubmission.VertexData.size() + 3);
                for (int i = 0; i < 3; ++i) {
                    TriangleVertexData *v = &currentSubmission.VertexData[baseVtx + i];
                    v->Position = instance.Positions[i];
                    v->TexCoords = instance.TexCoords[i];
                    v->InstanceIndex = instanceIndex;
                }
                currentSubmission.IndexData.resize(currentSubmission.IndexData.size() + 3);
                uint32_t *idx0 = &currentSubmission.IndexData[currentSubmission.IndexData.size() - 3];
                idx0[0] = baseVtx + 0;
                idx0[1] = baseVtx + 1;
                idx0[2] = baseVtx + 2;
            } else {
                // Quad (Assume TL, TR, BR, BL)
                currentSubmission.VertexData.resize(currentSubmission.VertexData.size() + 4);
                for (int i = 0; i < 4; ++i) {
                    TriangleVertexData *v = &currentSubmission.VertexData[baseVtx + i];
                    v->Position = instance.Positions[i];
                    v->TexCoords = instance.TexCoords[i];
                    v->InstanceIndex = instanceIndex;
                }
                currentSubmission.IndexData.resize(currentSubmission.IndexData.size() + 6);
                uint32_t *idx0 = &currentSubmission.IndexData[currentSubmission.IndexData.size() - 6];
                idx0[0] = baseVtx + 0;
                idx0[1] = baseVtx + 1;
                idx0[2] = baseVtx + 2;
                idx0[3] = baseVtx + 0;
                idx0[4] = baseVtx + 2;
                idx0[5] = baseVtx + 3;
            }
        }

        finalizeSubmission();

        return submissions;
    }

    void TriangleRenderingCommandList::GiveBackForNextFrame(std::vector<TriangleRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }

    void LineRenderingSubmissionData::Clear() {
        VertexData.clear();
    }

    void LineRenderingCommandList::Clear() {
        VertexData.clear();
    }


    void LineRenderingCommandList::AddLine(const glm::vec2 &p0, const glm::u8vec4 &color0,
                                           const glm::vec2 &p1, const glm::u8vec4 &color1) {
        VertexData.resize(VertexData.size() + 2);
        LineVertexData *v0 = &VertexData[VertexData.size() - 2];
        v0->Position = p0;

        v0->Color = (color0.r << 24) | (color0.g << 16) | (color0.b << 8) | color0.a;

        LineVertexData *v1 = &VertexData[VertexData.size() - 1];
        v1->Position = p1;
        v1->Color = (color1.r << 24) | (color1.g << 16) | (color1.b << 8) | color1.a;
    }


    std::vector<LineRenderingSubmissionData> LineRenderingCommandList::RecordRendererSubmissionData(
        size_t lineBufferInstanceSizeMax) {
        std::vector<LineRenderingSubmissionData> submissions;
        if (VertexData.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        LineRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.VertexData.clear();
            ++lastFrameSubmissionIt;
        }

        auto finalizeSubmission = [&]() mutable {
            if (!currentSubmission.VertexData.empty()) {
                submissions.push_back(std::move(currentSubmission));

                if (lastFrameSubmissionIt == mLastFrameCache.end()) {
                    currentSubmission.Clear();
                } else {
                    currentSubmission = std::move(*lastFrameSubmissionIt);
                    currentSubmission.VertexData.clear();
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &vertex: VertexData) {
            // check if we need to finalize due to vertex buffer size
            if (currentSubmission.VertexData.size() + 1 >
                lineBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            currentSubmission.VertexData.push_back(vertex);
        }

        finalizeSubmission();

        return submissions;
    }

    void LineRenderingCommandList::GiveBackForNextFrame(std::vector<LineRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }

    EllipseRenderingData EllipseRenderingData::Circle(const glm::vec2 &center, float radius,
                                                      const glm::u8vec4 &color, int depth,
                                                      const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Ellipse(const glm::vec2 &center, const glm::vec2 &radii,
                                                       float rotation, const glm::u8vec4 &color, int depth,
                                                       const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Ring(const glm::vec2 &center, float outerRadius, float innerRadius,
                                                    const glm::u8vec4 &color, int depth,
                                                    const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(outerRadius, outerRadius);
        data.InnerScale = innerRadius / outerRadius;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Sector(const glm::vec2 &center, float radius,
                                                      float startAngle, float endAngle,
                                                      const glm::u8vec4 &color, int textureIndex, int depth,
                                                      const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.VirtualTextureID = textureIndex;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Arc(const glm::vec2 &center, float radius, float thickness,
                                                   float startAngle, float endAngle,
                                                   const glm::u8vec4 &color, int depth,
                                                   const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.InnerScale = (radius - thickness) / radius;
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::EllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                                             float rotation, float startAngle, float endAngle,
                                                             const glm::u8vec4 &color, int textureIndex, int depth,
                                                             const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.VirtualTextureID = textureIndex;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    EllipseRenderingData EllipseRenderingData::EllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                                          float rotation, float thickness,
                                                          float startAngle, float endAngle,
                                                          const glm::u8vec4 &color, int depth,
                                                          const ClipRegion *clip) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        float minRadius = glm::min(radii.x, radii.y);
        data.InnerScale = glm::max(0.0f, (minRadius - thickness) / minRadius);
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.TintColor = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
        data.Depth = depth;
        if (clip != nullptr) {
            data.Clip = *clip;
        }
        return data;
    }

    void EllipseRenderingSubmissionData::Clear() {
        ShapeData.clear();
        ClipData.clear();
    }

    void EllipseRenderingCommandList::Clear() {
        Instances.clear();
    }

    void EllipseRenderingCommandList::AddEllipse(const EllipseRenderingData &data) {
        Instances.push_back(data);
    }

    std::vector<EllipseRenderingSubmissionData> EllipseRenderingCommandList::RecordRendererSubmissionData(
        size_t ellipseBufferInstanceSizeMax) {
        std::ranges::sort(Instances, [](const EllipseRenderingData &a, const EllipseRenderingData &b)-> bool {
            if (a.Depth != b.Depth) return a.Depth < b.Depth;
            return a.VirtualTextureID < b.VirtualTextureID;
        });


        std::vector<EllipseRenderingSubmissionData> submissions;
        if (Instances.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        EllipseRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.ShapeData.clear();
            currentSubmission.ClipData.clear();
            ++lastFrameSubmissionIt;
        }

        auto finalizeSubmission = [&]() mutable {
            if (!currentSubmission.ShapeData.empty()) {
                submissions.push_back(std::move(currentSubmission));

                if (lastFrameSubmissionIt == mLastFrameCache.end()) {
                    currentSubmission.Clear();
                } else {
                    currentSubmission = std::move(*lastFrameSubmissionIt);
                    currentSubmission.ShapeData.clear();
                    currentSubmission.ClipData.clear();
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &instance: Instances) {
            if (currentSubmission.ShapeData.size() + 1 > ellipseBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            // Handle clip region
            int32_t clipIndex = -1;
            if (instance.Clip.has_value()) {
                clipIndex = static_cast<int32_t>(currentSubmission.ClipData.size());
                currentSubmission.ClipData.push_back(instance.Clip.value());
            }

            EllipseShapeData shapeData;
            shapeData.Center = instance.Center;
            shapeData.Radii = instance.Radii;
            shapeData.Rotation = instance.Rotation;
            shapeData.InnerScale = instance.InnerScale;
            shapeData.StartAngle = instance.StartAngle;
            shapeData.EndAngle = instance.EndAngle;
            shapeData.TintColor = instance.TintColor;
            shapeData.TextureIndex = instance.VirtualTextureID;
            shapeData.EdgeSoftness = instance.EdgeSoftness;
            shapeData.ClipIndex = clipIndex;

            currentSubmission.ShapeData.push_back(shapeData);
        }

        finalizeSubmission();

        return submissions;
    }

    void EllipseRenderingCommandList::GiveBackForNextFrame(std::vector<EllipseRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }
}

#pragma endregion
