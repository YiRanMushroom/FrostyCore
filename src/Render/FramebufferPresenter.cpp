module Render.FramebufferPresenter;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;

namespace Engine {
    FramebufferPresenter::FramebufferPresenter(nvrhi::IDevice* device,
                                               const nvrhi::FramebufferInfo& targetFramebufferInfo)
        : mDevice(device) {
        CreateResources(targetFramebufferInfo);
    }

    void FramebufferPresenter::Present(nvrhi::ICommandList* commandList,
                                       nvrhi::ITexture* sourceTexture,
                                       nvrhi::IFramebuffer* targetFramebuffer) {
        nvrhi::BindingSetDesc setDesc;
        setDesc.bindings = {
            nvrhi::BindingSetItem::Texture_SRV(0, sourceTexture),
            nvrhi::BindingSetItem::Sampler(0, mSampler)
        };
        auto bindingSet = mDevice->createBindingSet(setDesc, mBindingLayout);

        commandList->setResourceStatesForFramebuffer(targetFramebuffer);
        commandList->setResourceStatesForBindingSet(bindingSet);

        nvrhi::GraphicsState state;
        state.pipeline = mPipeline;
        state.framebuffer = targetFramebuffer;
        state.bindings = {bindingSet};
        state.viewport.addViewportAndScissorRect(targetFramebuffer->getFramebufferInfo().getViewport());

        commandList->setGraphicsState(state);
        commandList->draw(nvrhi::DrawArguments().setVertexCount(3));
    }

    void FramebufferPresenter::CreateResources(const nvrhi::FramebufferInfo& targetFramebufferInfo) {
        mSampler = mDevice->createSampler(nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp)
            .setAllFilters(true));

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::Pixel;
        layoutDesc.bindings = {
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(0)
        };
        mBindingLayout = mDevice->createBindingLayout(layoutDesc);

        nvrhi::ShaderDesc vsDesc;
        vsDesc.shaderType = nvrhi::ShaderType::Vertex;
        vsDesc.entryName = "main";
        nvrhi::ShaderHandle vs = mDevice->createShader(vsDesc,
                                                       GeneratedShaders::copy_to_main_framebuffer_vs.data(),
                                                       GeneratedShaders::copy_to_main_framebuffer_vs.size());

        nvrhi::ShaderDesc psDesc;
        psDesc.shaderType = nvrhi::ShaderType::Pixel;
        psDesc.entryName = "main";
        nvrhi::ShaderHandle ps = mDevice->createShader(psDesc,
                                                       GeneratedShaders::copy_to_main_framebuffer_ps.data(),
                                                       GeneratedShaders::copy_to_main_framebuffer_ps.size());

        nvrhi::GraphicsPipelineDesc pipeDesc;
        pipeDesc.VS = vs;
        pipeDesc.PS = ps;
        pipeDesc.bindingLayouts = {mBindingLayout};
        pipeDesc.primType = nvrhi::PrimitiveType::TriangleList;

        pipeDesc.renderState.blendState.targets[0].blendEnable = true;
        pipeDesc.renderState.blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        pipeDesc.renderState.blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        pipeDesc.renderState.blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        pipeDesc.renderState.blendState.targets[0].colorWriteMask = nvrhi::ColorMask::All;

        pipeDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        pipeDesc.renderState.depthStencilState.depthTestEnable = false;

        mPipeline = mDevice->createGraphicsPipeline(pipeDesc, targetFramebufferInfo);
    }
}

