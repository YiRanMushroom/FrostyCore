export module Render.FramebufferPresenter;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;

namespace Engine {
    export class FramebufferPresenter {
    public:
        FramebufferPresenter(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& targetFramebufferInfo);

        void Present(nvrhi::ICommandList* commandList,
                    nvrhi::ITexture* sourceTexture,
                    nvrhi::IFramebuffer* targetFramebuffer);

    private:
        void CreateResources(const nvrhi::FramebufferInfo& targetFramebufferInfo);

        nvrhi::DeviceHandle mDevice;
        nvrhi::SamplerHandle mSampler;
        nvrhi::BindingLayoutHandle mBindingLayout;
        nvrhi::GraphicsPipelineHandle mPipeline;
    };
}

