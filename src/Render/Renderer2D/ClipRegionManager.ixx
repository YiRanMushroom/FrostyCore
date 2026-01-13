export module Render.Renderer2D:ClipRegionManager;

import :ForwardDecleration;
import :Misc;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace
Engine {
    export class ClipRegionManager {
    public:
        explicit ClipRegionManager(nvrhi::DeviceHandle device);

        // Register a clip region for this frame, returns the index
        // Returns -1 if region is null (no clipping)
        int RegisterClipRegion(const ClipRegion &region);

        // Get the index for "no clipping"
        static constexpr int NoClipping() { return -1; }

        // Clear all registered regions (call at the beginning of each frame)
        void ClearForNewFrame();

        // Prepare the buffer for rendering (copy all regions to GPU)
        void PrepareForRendering(nvrhi::ICommandList *commandList);

        // Get the current buffer handle
        [[nodiscard]] nvrhi::IBuffer *GetClipRegionBuffer() const;

        // Get the current count of registered regions
        [[nodiscard]] uint32_t GetRegisteredCount() const;

    private:
        void EnsureCapacity(size_t requiredCapacity);

        nvrhi::DeviceHandle mDevice;
        nvrhi::BufferHandle mClipRegionBuffer;
        std::vector<ClipRegion> mClipRegions;
        size_t mCurrentCapacity = 1024;
    };
}

