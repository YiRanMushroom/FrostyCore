module Render.Renderer2D;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;
import Render.VirtualTextureManager;
import Core.Coroutine;
import <cstddef>;

namespace
Engine {
    Renderer2D::Renderer2D(const Renderer2DDescriptor &desc, nvrhi::DeviceHandle device)
        : mDevice(device), mOutputSize(desc.OutputSize),
          mVirtualTextureManager(mDevice), mClipRegionManager(mDevice.Get()) {
        // mVirtualSize.x = desc.VirtualSizeWidth;
        // mVirtualSize.y = desc.VirtualSizeWidth * (
        // static_cast<float>(mOutputSize.y) / static_cast<float>(mOutputSize.x));
        CreateResources();
        CreateConstantBuffers();
        CreatePipelines();
        CreatePipelineResources();
        SetTransforms(desc.Transforms);
        for (auto &transform: mTransforms) {
            transform->OnFramebufferResized(static_cast<float>(mOutputSize.x),
                                            static_cast<float>(mOutputSize.y));
        }
    }

    void Renderer2D::CreatePipelineResources() {
        CreateTriangleBatchRenderingResources(4);
        // this should be enough for most cases, if not we can always expand it
        CreateLineBatchRenderingResources(4); // same for lines
        CreateEllipseBatchRenderingResources(4); // same for ellipses
    }

    void Renderer2D::BeginRendering(const Renderer2DBeginRenderingInfo &info) {
        Clear();

        mCommandList->open();

        mCommandList->setResourceStatesForFramebuffer(mFramebuffer);
        mCommandList->clearTextureFloat(mTexture,
                                        nvrhi::AllSubresources, info.ClearColor);
        mCommandList->clearTextureFloat(
            mTargetIDTexture,
            nvrhi::AllSubresources,
            nvrhi::Color(0.f, 0.f, 0.f, 0.f) // Clear to transparent (alpha=0)
        );
    }

    const nvrhi::CommandListHandle &Renderer2D::GetCommandList() const {
        return mCommandList;
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
        // CreatePipelineResources();
        // RecalculateViewProjectionMatrix();
        for (auto &transform: mTransforms) {
            transform->OnFramebufferResized(static_cast<float>(width), static_cast<float>(height));
        }
    }

    Awaitable<uint32_t> Renderer2D::GetEntityIDAtPixelPositionAsync(const glm::uvec2 &pixelPosition) {
        if (pixelPosition.x >= mOutputSize.x || pixelPosition.y >= mOutputSize.y) {
            co_return 0u;
        }

        nvrhi::TextureDesc stagingDesc;
        stagingDesc.width = 1;
        stagingDesc.height = 1;
        stagingDesc.format = nvrhi::Format::RGBA16_UNORM; // Use 16-bit per channel
        stagingDesc.debugName = "PickingStagingTexture";

        nvrhi::StagingTextureHandle stagingTex = mDevice->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);

        mCommandList->open();

        nvrhi::TextureSlice srcSlice;
        srcSlice.x = pixelPosition.x;
        srcSlice.y = pixelPosition.y;
        srcSlice.width = 1;
        srcSlice.height = 1;

        nvrhi::TextureSlice dstSlice;
        dstSlice.width = 1;
        dstSlice.height = 1;

        mCommandList->copyTexture(stagingTex, dstSlice, mTargetIDTexture, srcSlice);

        mCommandList->close();

        size_t submissionID = mDevice->executeCommandList(mCommandList);

        nvrhi::EventQueryHandle eventQuery = mDevice->createEventQuery();
        static_cast<nvrhi::vulkan::IDevice *>(mDevice.Get())->setEventQuery(
            eventQuery.Get(), nvrhi::CommandQueue::Graphics, submissionID);

        mDevice->waitEventQuery(eventQuery.Get());

        size_t outRowPitch = 0;

        void *mappedPtr = mDevice->mapStagingTexture(
            stagingTex,
            nvrhi::TextureSlice(0, 0, 0, 1, 1, 1),
            nvrhi::CpuAccessMode::Read,
            &outRowPitch
        );

        if (mappedPtr) {
            uint32_t entityID = 0;

            // Read as RGBA16_UNORM (8 bytes: 2 bytes per channel in little-endian)
            // Each channel is 16-bit, stored as little-endian uint16
            uint16_t* rgba16 = static_cast<uint16_t*>(mappedPtr);
            uint16_t r = rgba16[0];
            uint16_t g = rgba16[1];
            uint16_t b = rgba16[2];
            uint16_t a = rgba16[3];

            // Check alpha channel - if less than threshold, this pixel has no entity (EntityID=0)
            // For 16-bit: threshold is 0x8000 (half of 0xFFFF)
            if (a < 0x8000) {
                mDevice->unmapStagingTexture(stagingTex);
                co_return 0u;
            }

            // Decode EntityID from RG channels (32-bit = 16-bit R + 16-bit G)
            // R contains upper 16 bits, G contains lower 16 bits
            entityID = (static_cast<uint32_t>(r) << 16) | static_cast<uint32_t>(g);

            mDevice->unmapStagingTexture(stagingTex);
            co_return entityID;
        }

        co_return 0u;
    }

    void Renderer2D::CreateResources() {
        nvrhi::TextureDesc texDesc;
        texDesc.width = mOutputSize.x;
        texDesc.height = mOutputSize.y;
        texDesc.format = nvrhi::Format::RGBA8_UNORM;
        texDesc.isRenderTarget = true;
        texDesc.isShaderResource = true;
        // Needs to be sampled in another shader, so renderTarget and shaderResource
        texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        texDesc.keepInitialState = true;
        texDesc.clearValue = nvrhi::Color(0.f, 0.f, 0.f, 0.f);

        auto tex = mDevice->createTexture(texDesc);
        mTexture = tex;

        // Create Target ID texture, used for picking an object using a coordinate

        nvrhi::TextureDesc targetIDTexDesc;
        targetIDTexDesc.width = mOutputSize.x;
        targetIDTexDesc.height = mOutputSize.y;
        targetIDTexDesc.format = nvrhi::Format::RGBA16_UNORM; // Use 16-bit per channel to encode full 32-bit entity ID
        targetIDTexDesc.isRenderTarget = true;
        // should not be sampled in a shader, so not shader source
        targetIDTexDesc.isShaderResource = false;

        // we will render to it and read back on CPU, so it is renderTarget and copySource
        targetIDTexDesc.initialState = nvrhi::ResourceStates::CopySource;
        targetIDTexDesc.keepInitialState = true;
        // Clear to transparent (alpha=0) - preserves all values via blending
        targetIDTexDesc.clearValue = nvrhi::Color(0.f, 0.f, 0.f, 0.f);
        targetIDTexDesc.useClearValue = true;
        // We initialize it to 0 immediately

        auto targetIDTex = mDevice->createTexture(targetIDTexDesc);
        mTargetIDTexture = targetIDTex;

        // auto tempCommandList = mDevice->createCommandList();

        // tempCommandList->open();
        // tempCommandList->clearTextureUInt(
        //     mTargetIDTexture,
        //     nvrhi::AllSubresources,
        //     0u
        // );
        // tempCommandList->close();
        // mDevice->executeCommandList(tempCommandList);

        // two color attachments: 0 - color, 1 - target ID
        mFramebuffer = mDevice->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(tex).addColorAttachment(targetIDTex)
        );

        if (!mCommandList) {
            mCommandList = mDevice->createCommandList();
        }

        mTextureSampler = mDevice->createSampler(nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp)
            .setAllFilters(false));

        // font sampler should be linear
        mFontSampler = mDevice->createSampler(nvrhi::SamplerDesc()
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

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mTriangleConstantBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, resources.InstanceBuffer));
            bindingSetDesc.addItem(
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, mClipRegionManager.GetClipRegionBuffer()));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, mTextureSampler)); // point sampler
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(1, mFontSampler)); // linear sampler
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

            nvrhi::BindingSetDesc bindingSetDesc;
            bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, mEllipseConstantBuffer));
            bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, resources.ShapeBuffer));
            bindingSetDesc.addItem(
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, mClipRegionManager.GetClipRegionBuffer()));
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
            nvrhi::BindingLayoutItem::Sampler(0),
            nvrhi::BindingLayoutItem::Sampler(1)
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

        // Enable blending for second render target (EntityID)
        // When alpha = 0, preserve old value; when alpha = 255, write new value
        pipeDesc.renderState.blendState.targets[1].blendEnable = true;
        pipeDesc.renderState.blendState.targets[1].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[1].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[1].srcBlendAlpha = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[1].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[1].colorWriteMask = nvrhi::ColorMask::All;

        // nvrhi::RenderLayoutDesc layoutDesc;
        // layoutDesc.addColorAttachment(nvrhi::Format::RGBA8_UNORM);
        // layoutDesc.addColorAttachment(nvrhi::Format::RGBA8_UNORM);

        auto info = mFramebuffer->getFramebufferInfo();

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

        pipeDesc.renderState.blendState.targets[1].blendEnable = false;
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

        // Enable blending for second render target (EntityID)
        // When alpha = 0, preserve old value; when alpha = 255, write new value
        pipeDesc.renderState.blendState.targets[1].blendEnable = true;
        pipeDesc.renderState.blendState.targets[1].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[1].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[1].srcBlendAlpha = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[1].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[1].colorWriteMask = nvrhi::ColorMask::All;

        mEllipsePipeline = mDevice->createGraphicsPipeline(pipeDesc, mFramebuffer->getFramebufferInfo());
    }

    void Renderer2D::SubmitTriangleBatchRendering() {
        auto viewProjectionMatrix = GetViewProjectionMatrix();

        auto submissions = mTriangleCommandList.RecordRendererSubmissionData(
            mTriangleBufferInstanceSizeMax);

        CreateTriangleBatchRenderingResources(submissions.size());

        // submit constant buffer
        mCommandList->writeBuffer(mTriangleConstantBuffer, &viewProjectionMatrix,
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
        auto viewProjectionMatrix = GetViewProjectionMatrix();

        auto submissions = mLineCommandList.RecordRendererSubmissionData(
            mLineBufferVertexSizeMax);

        if (submissions.empty()) {
            return;
        }

        CreateLineBatchRenderingResources(submissions.size());

        // submit constant buffer
        mCommandList->writeBuffer(mLineConstantBuffer, &viewProjectionMatrix,
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
        auto viewProjectionMatrix = GetViewProjectionMatrix();

        auto submissions = mEllipseCommandList.RecordRendererSubmissionData(
            mEllipseBufferInstanceSizeMax);

        if (submissions.empty()) {
            return;
        }

        CreateEllipseBatchRenderingResources(submissions.size());

        mCommandList->writeBuffer(mEllipseConstantBuffer, &viewProjectionMatrix,
                                  sizeof(glm::mat4), 0);

        for (size_t i = 0; i < submissions.size(); ++i) {
            auto &submission = submissions[i];
            auto &resources = mEllipseBatchRenderingResources[i];

            if (submission.ShapeData.empty()) {
                continue;
            }

            mCommandList->writeBuffer(resources.ShapeBuffer, submission.ShapeData.data(),
                                      sizeof(EllipseShapeData) * submission.ShapeData.size(), 0);


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
        // Prepare clip region buffer before any rendering
        mClipRegionManager.PrepareForRendering(mCommandList.Get());

        SubmitTriangleBatchRendering();
        SubmitLineBatchRendering();
        SubmitEllipseBatchRendering();
    }

    glm::mat4 Renderer2D::GetViewProjectionMatrix() {
        glm::mat4 matrix(1.0f);
        for (auto &transform: mTransforms) {
            transform->DoTransform(matrix);
        }
        return matrix;
    }

    void Renderer2D::SetTransforms(std::vector<Ref<ITransform>> transforms) {
        mTransforms = std::move(transforms);
    }

    // void Renderer2D::RecalculateViewProjectionMatrix() {
    //     float scaleX = static_cast<float>(mOutputSize.x) / mVirtualSize.x;
    //     float scaleY = static_cast<float>(mOutputSize.y) / mVirtualSize.y;
    //
    //     float uniformScale = std::min(scaleX, scaleY);
    //
    //     float halfVisibleWidth = static_cast<float>(mOutputSize.x) / (2.0f * uniformScale);
    //     float halfVisibleHeight = static_cast<float>(mOutputSize.y) / (2.0f * uniformScale);
    //
    //     glm::mat4 projection = glm::ortho(-halfVisibleWidth, halfVisibleWidth,
    //         -halfVisibleHeight, halfVisibleHeight, -1.f, 1.f);
    //
    //     // Now this projection is in Vulkan NDC space, we need to convert it to OpenGL NDC space
    //     // to account for nvrhi using OpenGL/DirectX style NDC with Y up
    //     // constexpr glm::mat4 adaptNDC = glm::mat4(
    //     //     -1.0f,  0.0f, 0.0f, 0.0f,
    //     //      0.0f, -1.0f, 0.0f, 0.0f,
    //     //      0.0f,  0.0f, 1.0f, 0.0f,
    //     //      0.0f,  0.0f, 0.0f, 1.0f
    //     // );
    //
    //     // mViewProjectionMatrix = adaptNDC * projection;
    //
    //     mViewProjectionMatrix = projection;
    // }

    void VirtualSizeTransform::DoTransform(glm::mat4 &matrix) {
        if (!mCachedTransform.has_value()) {
            float scaleX = mFramebufferSize.x / mVirtualSize.x;
            float scaleY = mFramebufferSize.y / mVirtualSize.y;

            float uniformScale = std::min(scaleX, scaleY);

            float halfVisibleWidth = mFramebufferSize.x / (2.0f * uniformScale);
            float halfVisibleHeight = mFramebufferSize.y / (2.0f * uniformScale);

            // With GLM_FORCE_DEPTH_ZERO_TO_ONE, glm::ortho maps near->0.0 and far->1.0 in Vulkan
            // We use a symmetric range around 0 to allow depth variations from transformations
            glm::mat4 projection = glm::ortho(-halfVisibleWidth, halfVisibleWidth,
                                              -halfVisibleHeight, halfVisibleHeight, -10000.f, 10000.f);

            mCachedTransform = {projection};
        }

        matrix = (*mCachedTransform) * matrix;
    }

    nvrhi::ITexture *Renderer2D::GetTexture() const {
        return mTexture.Get();
    }

    void Renderer2D::Clear() {
        mTriangleCommandList.Clear();
        mLineCommandList.Clear();
        mEllipseCommandList.Clear();
        mClipRegionManager.ClearForNewFrame();
    }

    uint32_t Renderer2D::RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle &texture) {
        return mVirtualTextureManager.RegisterTexture(texture);
    }

    ClipRegionManager &Renderer2D::GetClipRegionManager() {
        return mClipRegionManager;
    }

    int Renderer2D::GetCurrentDepth() const {
        return mCurrentDepth;
    }

    void Renderer2D::SetCurrentDepth(int depth) {
        mCurrentDepth = depth;
    }

    void Renderer2D::DrawTriangleColored(const glm::mat3x2 &positions,
                                         const glm::u8vec4 &color,
                                         std::optional<int> overrideDepth,
                                         int clipRegionId) {
        mTriangleCommandList.AddTriangle(
            positions[0], glm::vec2(0.f, 0.f),
            positions[1], glm::vec2(0.f, 0.f),
            positions[2], glm::vec2(0.f, 0.f),
            -1,
            color,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
    }

    void Renderer2D::DrawTriangleTextureVirtual(const glm::mat3x2 &positions,
                                                const glm::mat3x2 &uvs,
                                                uint32_t virtualTextureID,
                                                std::optional<int> overrideDepth,
                                                glm::u8vec4 tintColor, int clipRegionId) {
        mTriangleCommandList.AddTriangle(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            static_cast<int>(virtualTextureID),
            tintColor,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
    }

    uint32_t Renderer2D::DrawTriangleTextureManaged(const glm::mat3x2 &positions,
                                                    const glm::mat3x2 &uvs,
                                                    const nvrhi::TextureHandle &texture,
                                                    std::optional<int> overrideDepth,
                                                    glm::u8vec4 tintColor, int clipRegionId) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        mTriangleCommandList.AddTriangle(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            static_cast<int>(virtualTextureID),
            tintColor,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
        return virtualTextureID;
    }

    void Renderer2D::DrawQuadColored(const glm::mat4x2 &positions,
                                     const glm::u8vec4 &color,
                                     std::optional<int> overrideDepth, int clipRegionId) {
        mTriangleCommandList.AddQuad(
            positions[0], glm::vec2(0.f, 0.f),
            positions[1], glm::vec2(0.f, 0.f),
            positions[2], glm::vec2(0.f, 0.f),
            positions[3], glm::vec2(0.f, 0.f),
            -1,
            color,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
    }

    void Renderer2D::DrawQuadTextureVirtual(const glm::mat4x2 &positions,
                                            const glm::mat4x2 &uvs,
                                            uint32_t virtualTextureID,
                                            std::optional<int> overrideDepth,
                                            glm::u8vec4 tintColor, int clipRegionId) {
        mTriangleCommandList.AddQuad(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            positions[3], uvs[3],
            static_cast<int>(virtualTextureID),
            tintColor,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
    }

    uint32_t Renderer2D::DrawQuadTextureManaged(const glm::mat4x2 &positions,
                                                const glm::mat4x2 &uvs,
                                                const nvrhi::TextureHandle &texture,
                                                std::optional<int> overrideDepth,
                                                glm::u8vec4 tintColor, int clipRegionId) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        mTriangleCommandList.AddQuad(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            positions[3], uvs[3],
            static_cast<int>(virtualTextureID),
            tintColor,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth, clipRegionId);
        return virtualTextureID;
    }

    void Renderer2D::DrawQuadFontColoredVirtual(const glm::mat4x2 &positions, const glm::mat4x2 &uvs,
                                                uint32_t virtualTextureID, glm::u8vec4 tintColor, float MTSDFPixelRange,
                                                std::optional<int> overrideDepth,
                                                int clipRegionId) {
        mTriangleCommandList.AddQuadFont(
            positions[0], uvs[0],
            positions[1], uvs[1],
            positions[2], uvs[2],
            positions[3], uvs[3],
            static_cast<int>(virtualTextureID),
            tintColor,
            MTSDFPixelRange,
            overrideDepth.has_value() ? overrideDepth.value() : mCurrentDepth,
            clipRegionId);
    }

    void Renderer2D::DrawLine(const glm::vec2 &p0, const glm::vec2 &p1,
                              const glm::u8vec4 &color) {
        mLineCommandList.AddLine(p0, p1, color);
    }

    void Renderer2D::DrawCircle(const glm::vec2 &center, float radius,
                                const glm::u8vec4 &color,
                                std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Circle(
            center, radius, color, overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipse(const glm::vec2 &center, const glm::vec2 &radii,
                                 float rotation, const glm::u8vec4 &color,
                                 std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Ellipse(
            center, radii, rotation, color, overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawRing(const glm::vec2 &center, float outerRadius, float innerRadius,
                              const glm::u8vec4 &color,
                              std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Ring(
            center, outerRadius, innerRadius, color, overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawSector(const glm::vec2 &center, float radius,
                                float startAngle, float endAngle,
                                const glm::u8vec4 &color,
                                std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, color, -1, overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawSectorTextureVirtual(const glm::vec2 &center, float radius,
                                              float startAngle, float endAngle,
                                              uint32_t virtualTextureID,
                                              const glm::u8vec4 &tintColor,
                                              std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawSectorTextureManaged(const glm::vec2 &center, float radius,
                                                  float startAngle, float endAngle,
                                                  const nvrhi::TextureHandle &texture,
                                                  const glm::u8vec4 &tintColor,
                                                  std::optional<int> overrideDepth, int clipRegionId) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        EllipseRenderingData data = EllipseRenderingData::Sector(
            center, radius, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
        return virtualTextureID;
    }

    void Renderer2D::DrawArc(const glm::vec2 &center, float radius, float thickness,
                             float startAngle, float endAngle,
                             const glm::u8vec4 &color,
                             std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::Arc(
            center, radius, thickness, startAngle, endAngle, color, overrideDepth.value_or(mCurrentDepth),
            clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                       float rotation, float startAngle, float endAngle,
                                       const glm::u8vec4 &color,
                                       std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::EllipseSector(
            center, radii, rotation, startAngle, endAngle, color, -1, overrideDepth.value_or(mCurrentDepth),
            clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseSectorTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii,
                                                     float rotation, float startAngle, float endAngle,
                                                     uint32_t virtualTextureID,
                                                     const glm::u8vec4 &tintColor,
                                                     std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::EllipseSector(
            center, radii, rotation, startAngle, endAngle, tintColor,
            static_cast<int>(virtualTextureID), overrideDepth.value_or(mCurrentDepth), clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawEllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                    float rotation, float thickness,
                                    float startAngle, float endAngle,
                                    const glm::u8vec4 &color,
                                    std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data = EllipseRenderingData::EllipseArc(
            center, radii, rotation, thickness, startAngle, endAngle, color, overrideDepth.value_or(mCurrentDepth),
            clipRegionId);
        mEllipseCommandList.AddEllipse(data);
    }

    void Renderer2D::DrawCircleTextureVirtual(const glm::vec2 &center, float radius,
                                              uint32_t virtualTextureID,
                                              const glm::u8vec4 &tintColor,
                                              std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.VirtualTextureID = static_cast<int>(virtualTextureID);
        data.TintColor = tintColor;
        data.Depth = overrideDepth.value_or(mCurrentDepth);
        data.ClipRegionId = clipRegionId;
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawCircleTextureManaged(const glm::vec2 &center, float radius,
                                                  const nvrhi::TextureHandle &texture,
                                                  const glm::u8vec4 &tintColor,
                                                  std::optional<int> overrideDepth, int clipRegionId) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        DrawCircleTextureVirtual(center, radius, virtualTextureID, tintColor, overrideDepth, clipRegionId);
        return virtualTextureID;
    }

    void Renderer2D::DrawEllipseTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii,
                                               float rotation, uint32_t virtualTextureID,
                                               const glm::u8vec4 &tintColor,
                                               std::optional<int> overrideDepth, int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.VirtualTextureID = static_cast<int>(virtualTextureID);
        data.TintColor = tintColor;
        data.Depth = overrideDepth.value_or(mCurrentDepth);
        data.ClipRegionId = clipRegionId;
        mEllipseCommandList.AddEllipse(data);
    }

    uint32_t Renderer2D::DrawEllipseTextureManaged(const glm::vec2 &center, const glm::vec2 &radii,
                                                   float rotation,
                                                   const nvrhi::TextureHandle &texture,
                                                   const glm::u8vec4 &tintColor,
                                                   std::optional<int> overrideDepth, int clipRegionId) {
        uint32_t virtualTextureID = RegisterVirtualTextureForThisFrame(texture);
        DrawEllipseTextureVirtual(center, radii, rotation, virtualTextureID, tintColor, overrideDepth, clipRegionId);
        return virtualTextureID;
    }

    template<>
    void Renderer2D::Draw<TriangleDrawCommand>(const TriangleDrawCommand &command) {
        // mTriangleCommandList.AddTriangle(
        //     command.Positions[0],
        //     command.UVs[0],
        //     command.Positions[1],
        //     command.UVs[1],
        //     command.Positions[2],
        //     command.UVs[2],
        //     command.VirtualTextureID,
        //     command.TintColor,
        //     command.OverrideDepth.value_or(mCurrentDepth),
        //     command.ClipRegionId
        // );
        mTriangleCommandList.Instances.push_back(
            TriangleRenderingData{
                .Positions = glm::mat4x2(command.Positions[0], command.Positions[1], command.Positions[2],
                                         glm::vec2(0.0f, 0.0f)),
                .TexCoords = glm::mat4x2(command.UVs[0], command.UVs[1], command.UVs[2], glm::vec2(0.0f, 0.0f)),
                .IsQuad = false,
                .VirtualTextureID = command.VirtualTextureID,
                .TintColor = command.TintColor,
                .Depth = command.OverrideDepth.value_or(mCurrentDepth),
                .ClipRegionId = command.ClipRegionId,
                .EntityID = command.EntityID,
                .RenderingMode = InstanceRenderingMode::Texture,
                .MTSDFPixelRange = 0.0f,
                .ModelMatrix = command.ModelMatrix
            }
        );
    }

    template<>
    void Renderer2D::Draw<>(const QuadDrawCommand &command) {
        if (command.RenderingMode == InstanceRenderingMode::Texture) {
            // mTriangleCommandList.AddQuad(
            //     command.FirstPoint, command.FirstUV,
            //     glm::vec2(command.SecondPoint.x, command.FirstPoint.y),
            //     glm::vec2(command.SecondUV.x, command.FirstUV.y),
            //     command.SecondPoint, command.SecondUV,
            //     glm::vec2(command.FirstPoint.x, command.SecondPoint.y),
            //     glm::vec2(command.FirstUV.x, command.SecondUV.y),
            //     command.VirtualTextureID,
            //     command.TintColor,
            //     command.OverrideDepth.value_or(mCurrentDepth),
            //     command.ClipRegionId
            // );
            mTriangleCommandList.Instances.push_back(
                TriangleRenderingData{
                    .Positions = glm::mat4x2(
                        command.FirstPoint,
                        glm::vec2(command.SecondPoint.x, command.FirstPoint.y),
                        command.SecondPoint,
                        glm::vec2(command.FirstPoint.x, command.SecondPoint.y)
                    ),
                    .TexCoords = glm::mat4x2(
                        command.FirstUV,
                        glm::vec2(command.SecondUV.x, command.FirstUV.y),
                        command.SecondUV,
                        glm::vec2(command.FirstUV.x, command.SecondUV.y)
                    ),
                    .IsQuad = true,
                    .VirtualTextureID = command.VirtualTextureID,
                    .TintColor = command.TintColor,
                    .Depth = command.OverrideDepth.value_or(mCurrentDepth),
                    .ClipRegionId = command.ClipRegionId,
                    .EntityID = command.EntityID,
                    .RenderingMode = InstanceRenderingMode::Texture,
                    .MTSDFPixelRange = 0.0f,
                    .ModelMatrix = command.ModelMatrix
                }
            );
        } else if (command.RenderingMode == InstanceRenderingMode::MTSDF) {
            // mTriangleCommandList.AddQuadFont(
            //     command.FirstPoint, command.FirstUV,
            //     glm::vec2(command.SecondPoint.x, command.FirstPoint.y),
            //     glm::vec2(command.SecondUV.x, command.FirstUV.y),
            //     command.SecondPoint, command.SecondUV,
            //     glm::vec2(command.FirstPoint.x, command.SecondPoint.y),
            //     glm::vec2(command.FirstUV.x, command.SecondUV.y),
            //     command.VirtualTextureID,
            //     command.TintColor,
            //     command.MTSDFPixelRange,
            //     command.OverrideDepth.value_or(mCurrentDepth),
            //     command.ClipRegionId
            // );
            mTriangleCommandList.Instances.push_back(
                TriangleRenderingData{
                    .Positions = glm::mat4x2(
                        command.FirstPoint,
                        glm::vec2(command.SecondPoint.x, command.FirstPoint.y),
                        command.SecondPoint,
                        glm::vec2(command.FirstPoint.x, command.SecondPoint.y)
                    ),
                    .TexCoords = glm::mat4x2(
                        command.FirstUV,
                        glm::vec2(command.SecondUV.x, command.FirstUV.y),
                        command.SecondUV,
                        glm::vec2(command.FirstUV.x, command.SecondUV.y)
                    ),
                    .IsQuad = true,
                    .VirtualTextureID = command.VirtualTextureID,
                    .TintColor = command.TintColor,
                    .Depth = command.OverrideDepth.value_or(mCurrentDepth),
                    .ClipRegionId = command.ClipRegionId,
                    .EntityID = command.EntityID,
                    .RenderingMode = InstanceRenderingMode::MTSDF,
                    .MTSDFPixelRange = command.MTSDFPixelRange,
                    .ModelMatrix = command.ModelMatrix
                }
            );
        }
    }

    template<>
    void Renderer2D::Draw<>(const CircularDrawCommand &command) {
        mEllipseCommandList.Instances.push_back({
            .Center = command.Center,
            .Radii = command.Radii,
            .Rotation = command.Rotation,
            .InnerScale = command.InnerScale,
            .StartAngle = command.StartAngle,
            .EndAngle = command.EndAngle,
            .VirtualTextureID = command.VirtualTextureID,
            .TintColor = command.TintColor,
            .EdgeSoftness = command.EdgeSoftness,
            .Depth = command.OverrideDepth.value_or(mCurrentDepth),
            .ClipRegionId = command.ClipRegionId,
            .EntityID = command.EntityID,
            .ModelMatrix = command.ModelMatrix
        });
    }
}
