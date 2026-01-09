module Render.VirtualTextureManager;

import Vendor.ApplicationAPI;
import Core.Prelude;

namespace Engine {
    VirtualTextureManager::VirtualTextureManager(nvrhi::IDevice* device, uint32_t initialMax)
        : mDevice(device), mMaxTextures(initialMax) {
        mBindingSetDesc.bindings.reserve(mMaxTextures);
    }

    uint32_t VirtualTextureManager::RegisterTexture(nvrhi::TextureHandle texture) {
        if (!texture) return static_cast<uint32_t>(-1);

        auto it = mTextureToVirtualID.find(texture.Get());
        if (it != mTextureToVirtualID.end()) {
            return it->second;
        }

        if (mVirtualTextures.size() >= mMaxTextures) {
            throw Engine::RuntimeException(
                "VirtualTextureManager: Capacity reached. Call Optimize() or increase limit.");
        }

        uint32_t newID = static_cast<uint32_t>(mVirtualTextures.size());
        mVirtualTextures.push_back(texture);
        mTextureToVirtualID[texture.Get()] = newID;

        mBindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture)
            .setArrayElement(newID));

        mIsDirty = true;
        return newID;
    }

    void VirtualTextureManager::Optimize() {
        uint32_t hardLimit = 1 << 18;
        if (mMaxTextures < hardLimit) {
            mMaxTextures = std::min(mMaxTextures * 2, hardLimit);
        }

        Reset();
    }

    nvrhi::BindingSetHandle VirtualTextureManager::GetBindingSet(nvrhi::IBindingLayout* layout) {
        if (mVirtualTextures.empty()) {
            return nullptr;
        }

        if (mIsDirty || !mCurrentBindingSet) {
            mCurrentBindingSet = mDevice->createBindingSet(mBindingSetDesc, layout);
            mIsDirty = false;
        }

        return mCurrentBindingSet;
    }

    bool VirtualTextureManager::IsSubOptimal() const {
        return mVirtualTextures.size() >= mMaxTextures * 3 / 4;
    }

    void VirtualTextureManager::Reset() {
        mVirtualTextures.clear();
        mTextureToVirtualID.clear();
        mBindingSetDesc.bindings.clear();
        mBindingSetDesc.bindings.reserve(mMaxTextures);
        mCurrentBindingSet = nullptr;
        mIsDirty = true;
    }

    uint32_t VirtualTextureManager::GetCurrentSize() const {
        return static_cast<uint32_t>(mVirtualTextures.size());
    }

    uint32_t VirtualTextureManager::GetCapacity() const {
        return mMaxTextures;
    }
}

