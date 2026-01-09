export module Render.VirtualTextureManager;

import Vendor.ApplicationAPI;
import Core.Prelude;

namespace Engine {
    export class VirtualTextureManager {
    public:
        explicit VirtualTextureManager(nvrhi::IDevice* device, uint32_t initialMax = 65536);

        uint32_t RegisterTexture(nvrhi::TextureHandle texture);

        void Optimize();

        nvrhi::BindingSetHandle GetBindingSet(nvrhi::IBindingLayout* layout);

        [[nodiscard]] bool IsSubOptimal() const;

        void Reset();

        [[nodiscard]] uint32_t GetCurrentSize() const;
        [[nodiscard]] uint32_t GetCapacity() const;

    private:
        nvrhi::DeviceHandle mDevice;
        uint32_t mMaxTextures;

        std::vector<nvrhi::TextureHandle> mVirtualTextures;
        std::unordered_map<nvrhi::ITexture*, uint32_t> mTextureToVirtualID;

        nvrhi::BindingSetDesc mBindingSetDesc;
        nvrhi::BindingSetHandle mCurrentBindingSet;

        bool mIsDirty = true;
    };
}

