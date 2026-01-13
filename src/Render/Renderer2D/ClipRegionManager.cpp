module Render.Renderer2D;

import :ClipRegionManager;
import :Misc;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace
Engine {
    ClipRegionManager::ClipRegionManager(nvrhi::DeviceHandle device)
        : mDevice(device) {
        mClipRegionBuffer = mDevice->createBuffer({
            .byteSize = mCurrentCapacity * sizeof(ClipRegion),
            .structStride = sizeof(ClipRegion),
            .debugName = "ClipRegionBuffer",
            .canHaveUAVs = false,
            .isVertexBuffer = false,
            .isDrawIndirectArgs = false,
            .initialState = nvrhi::ResourceStates::ShaderResource,
            .keepInitialState = true
        });
    }

    int ClipRegionManager::RegisterClipRegion(const ClipRegion &region) {
        mClipRegions.emplace_back(region);
        return static_cast<int>(mClipRegions.size() - 1);
    }

    void ClipRegionManager::ClearForNewFrame() {
        mClipRegions.clear();
    }

    void ClipRegionManager::PrepareForRendering(nvrhi::ICommandList *commandList) {
        if (mClipRegions.empty()) {
            return;
        }

        // Ensure capacity
        EnsureCapacity(mClipRegions.size());

        // Upload all regions to the buffer
        commandList->writeBuffer(mClipRegionBuffer, mClipRegions.data(),
                                 mClipRegions.size() * sizeof(ClipRegion));
    }

    nvrhi::IBuffer *ClipRegionManager::GetClipRegionBuffer() const {
        return mClipRegionBuffer;
    }

    uint32_t ClipRegionManager::GetRegisteredCount() const {
        return static_cast<uint32_t>(mClipRegions.size());
    }

    void ClipRegionManager::EnsureCapacity(size_t requiredCapacity) {
        if (requiredCapacity <= mCurrentCapacity) {
            return; // Already have enough capacity
        }

        // Double the capacity until it's enough
        while (mCurrentCapacity < requiredCapacity) {
            mCurrentCapacity *= 2;
        }

        // Create a new buffer with the increased capacity
        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = mCurrentCapacity * sizeof(ClipRegion);
        bufferDesc.structStride = sizeof(ClipRegion);
        bufferDesc.canHaveUAVs = false;
        bufferDesc.debugName = "ClipRegionBuffer";
        bufferDesc.isVertexBuffer = false;
        bufferDesc.isDrawIndirectArgs = false;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        bufferDesc.keepInitialState = true;

        mClipRegionBuffer = mDevice->createBuffer(bufferDesc);
    }
}

