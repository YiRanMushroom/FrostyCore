export module Render.Swapchain;

import Vendor.ApplicationAPI;
import Core.Prelude;
import <cstdint>;

namespace Engine {
    /// Result of acquiring a swapchain image
    export struct SwapchainAcquireResult {
        uint32_t imageIndex = UINT32_MAX;
        vk::Result result = vk::Result::eSuccess;

        [[nodiscard]] bool IsSuccess() const {
            return result == vk::Result::eSuccess;
        }

        [[nodiscard]] bool NeedsRecreation() const {
            return result == vk::Result::eErrorOutOfDateKHR ||
                   result == vk::Result::eSuboptimalKHR;
        }

        [[nodiscard]] bool IsValid() const {
            return imageIndex != UINT32_MAX;
        }
    };

    /// Encapsulates Vulkan swapchain with synchronization primitives
    /// Separates swapchain image management from frame-in-flight synchronization
    export class PlatformSwapchain {
    public:
        PlatformSwapchain() = default;

        /// Create swapchain with all necessary resources
        PlatformSwapchain(SDL_Window* window,
                          const vk::SharedSurfaceKHR& platformSurface,
                          const vk::SharedPhysicalDevice& physicalDevice,
                          const vk::SharedDevice& device,
                          const nvrhi::vulkan::DeviceHandle& nvrhiDevice,
                          vk::SwapchainKHR oldSwapchain = nullptr);

        /// Recreate swapchain (e.g., on window resize)
        void Recreate(SDL_Window* window,
                      const vk::SharedSurfaceKHR& platformSurface,
                      const vk::SharedPhysicalDevice& physicalDevice,
                      const vk::SharedDevice& device,
                      const nvrhi::vulkan::DeviceHandle& nvrhiDevice);

        // ============================================
        // Core Swapchain Info
        // ============================================

        /// Get the underlying Vulkan swapchain handle
        [[nodiscard]] const vk::SharedSwapchainKHR& GetSwapchain() const { return mSwapchain; }

        /// Get the number of swapchain images
        [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(mBackBuffers.size()); }

        /// Get swapchain width in pixels
        [[nodiscard]] uint32_t GetWidth() const { return mWidth; }

        /// Get swapchain height in pixels
        [[nodiscard]] uint32_t GetHeight() const { return mHeight; }

        /// Get swapchain format (NVRHI)
        [[nodiscard]] nvrhi::Format GetFormat() const { return mFormat; }

        /// Get swapchain format (Vulkan)
        [[nodiscard]] vk::Format GetVkFormat() const {
            return static_cast<vk::Format>(nvrhi::vulkan::convertFormat(mFormat));
        }

        /// Get NVRHI format from framebuffers (safe getter)
        [[nodiscard]] nvrhi::Format GetNvrhiFormat() const {
            return mFramebuffers.empty() ? nvrhi::Format::UNKNOWN
                                         : mFramebuffers[0]->getFramebufferInfo().colorFormats[0];
        }

        // ============================================
        // Image Access
        // ============================================

        /// Get all Vulkan shared images
        [[nodiscard]] const std::vector<vk::SharedImage>& GetSwapchainImages() const { return mSwapchainImages; }

        /// Get specific Vulkan image by index
        [[nodiscard]] const vk::SharedImage& GetSwapchainImage(uint32_t index) const { return mSwapchainImages[index]; }

        /// Get all NVRHI back buffer textures
        [[nodiscard]] const std::vector<nvrhi::TextureHandle>& GetBackBuffers() const { return mBackBuffers; }

        /// Get specific back buffer by index
        [[nodiscard]] const nvrhi::TextureHandle& GetBackBuffer(uint32_t index) const { return mBackBuffers[index]; }

        /// Get all NVRHI framebuffers
        [[nodiscard]] const std::vector<nvrhi::FramebufferHandle>& GetFramebuffers() const { return mFramebuffers; }

        /// Get specific framebuffer by index
        [[nodiscard]] const nvrhi::FramebufferHandle& GetFramebuffer(uint32_t index) const { return mFramebuffers[index]; }

        /// Get current acquired image index (valid only after successful AcquireNextImage)
        [[nodiscard]] uint32_t GetCurrentImageIndex() const { return mCurrentImageIndex; }

        /// Get current framebuffer (valid only after successful AcquireNextImage)
        [[nodiscard]] const nvrhi::FramebufferHandle& GetCurrentFramebuffer() const {
            return mFramebuffers[mCurrentImageIndex];
        }

        /// Get current back buffer (valid only after successful AcquireNextImage)
        [[nodiscard]] const nvrhi::TextureHandle& GetCurrentBackBuffer() const {
            return mBackBuffers[mCurrentImageIndex];
        }

        // ============================================
        // Synchronization - Semaphores
        // ============================================

        /// Get render complete semaphore for a specific image
        [[nodiscard]] const vk::SharedSemaphore& GetRenderCompleteSemaphore(uint32_t imageIndex) const {
            return mRenderCompleteSemaphores[imageIndex];
        }

        /// Get current image's render complete semaphore
        [[nodiscard]] const vk::SharedSemaphore& GetCurrentRenderCompleteSemaphore() const {
            return mRenderCompleteSemaphores[mCurrentImageIndex];
        }

        /// Get all render complete semaphores
        [[nodiscard]] const std::vector<vk::SharedSemaphore>& GetRenderCompleteSemaphores() const {
            return mRenderCompleteSemaphores;
        }

        // ============================================
        // Image Acquisition and Presentation
        // ============================================

        /// Acquire next swapchain image
        /// @param acquireSemaphore Semaphore to signal when image is ready (managed externally, typically per frame-in-flight)
        /// @param timeout Timeout in nanoseconds (default: infinite)
        /// @return Result containing image index and vk::Result
        SwapchainAcquireResult AcquireNextImage(vk::Semaphore acquireSemaphore,
                                                 uint64_t timeout = UINT64_MAX);

        /// Present the current image
        /// @param queue The queue to present on
        /// @return vk::Result of the present operation
        vk::Result Present(const vk::SharedQueue& queue);

        /// Present a specific image
        /// @param queue The queue to present on
        /// @param imageIndex The image index to present
        /// @return vk::Result of the present operation
        vk::Result Present(const vk::SharedQueue& queue, uint32_t imageIndex);

        // ============================================
        // State Query
        // ============================================

        /// Check if swapchain is valid
        [[nodiscard]] bool IsValid() const { return mSwapchain && !mBackBuffers.empty(); }

        /// Check if we have a currently acquired image
        [[nodiscard]] bool HasAcquiredImage() const { return mCurrentImageIndex != UINT32_MAX; }

        /// Reset acquired image state (call after present)
        void ResetAcquiredState() { mCurrentImageIndex = UINT32_MAX; }

    private:
        static PlatformSwapchain CreateSwapchainInternal(
            SDL_Window* window,
            const vk::SharedSurfaceKHR& platformSurface,
            const vk::SharedPhysicalDevice& physicalDevice,
            const vk::SharedDevice& device,
            const nvrhi::vulkan::DeviceHandle& nvrhiDevice,
            vk::SwapchainKHR oldSwapchain = nullptr);

    private:
        vk::SharedSwapchainKHR mSwapchain;
        vk::SharedDevice mDevice; // Keep device reference for acquire/present operations
        std::vector<vk::SharedImage> mSwapchainImages;
        std::vector<nvrhi::TextureHandle> mBackBuffers;
        std::vector<nvrhi::FramebufferHandle> mFramebuffers;

        // Per-image render complete semaphores
        std::vector<vk::SharedSemaphore> mRenderCompleteSemaphores;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        nvrhi::Format mFormat = nvrhi::Format::UNKNOWN;

        // Current acquired image index (UINT32_MAX if no image acquired)
        uint32_t mCurrentImageIndex = UINT32_MAX;
    };

    // ========================================================================
    // Implementation
    // ========================================================================

    inline PlatformSwapchain::PlatformSwapchain(
        SDL_Window* window,
        const vk::SharedSurfaceKHR& platformSurface,
        const vk::SharedPhysicalDevice& physicalDevice,
        const vk::SharedDevice& device,
        const nvrhi::vulkan::DeviceHandle& nvrhiDevice,
        vk::SwapchainKHR oldSwapchain) {
        *this = CreateSwapchainInternal(window, platformSurface, physicalDevice, device, nvrhiDevice, oldSwapchain);
    }

    inline void PlatformSwapchain::Recreate(
        SDL_Window* window,
        const vk::SharedSurfaceKHR& platformSurface,
        const vk::SharedPhysicalDevice& physicalDevice,
        const vk::SharedDevice& device,
        const nvrhi::vulkan::DeviceHandle& nvrhiDevice) {
        device->waitIdle();
        vk::SwapchainKHR oldSwapchain = mSwapchain ? mSwapchain.get() : nullptr;
        *this = CreateSwapchainInternal(window, platformSurface, physicalDevice, device, nvrhiDevice, oldSwapchain);
    }

    inline SwapchainAcquireResult PlatformSwapchain::AcquireNextImage(
        vk::Semaphore acquireSemaphore,
        uint64_t timeout) {
        SwapchainAcquireResult result;

        uint32_t imageIndex = 0;
        vk::Result acquireResult = mDevice.get().acquireNextImageKHR(
            mSwapchain.get(),
            timeout,
            acquireSemaphore,
            nullptr,
            &imageIndex
        );

        result.result = acquireResult;

        if (acquireResult == vk::Result::eSuccess || acquireResult == vk::Result::eSuboptimalKHR) {
            result.imageIndex = imageIndex;
            mCurrentImageIndex = imageIndex;
        }

        return result;
    }

    inline vk::Result PlatformSwapchain::Present(const vk::SharedQueue& queue) {
        return Present(queue, mCurrentImageIndex);
    }

    inline vk::Result PlatformSwapchain::Present(const vk::SharedQueue& queue, uint32_t imageIndex) {
        vk::SwapchainKHR rawSwapchain = mSwapchain.get();
        vk::Semaphore waitSemaphores[] = { mRenderCompleteSemaphores[imageIndex].get() };

        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &rawSwapchain;
        presentInfo.pImageIndices = &imageIndex;

        return queue.get().presentKHR(presentInfo);
    }

    inline PlatformSwapchain PlatformSwapchain::CreateSwapchainInternal(
        SDL_Window* window,
        const vk::SharedSurfaceKHR& platformSurface,
        const vk::SharedPhysicalDevice& physicalDevice,
        const vk::SharedDevice& device,
        const nvrhi::vulkan::DeviceHandle& nvrhiDevice,
        vk::SwapchainKHR oldSwapchain) {

        vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.get().getSurfaceCapabilitiesKHR(platformSurface.get());

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        uint32_t imageCount = std::min(capabilities.minImageCount + 1,
                                       capabilities.maxImageCount > 0 ? capabilities.maxImageCount : UINT32_MAX);

        vk::SwapchainCreateInfoKHR swapchainCreationInfo = vk::SwapchainCreateInfoKHR()
            .setSurface(platformSurface.get())
            .setMinImageCount(imageCount)
            .setImageFormat(vk::Format::eB8G8R8A8Unorm)  // More common format
            .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
            .setImageExtent(vk::Extent2D(static_cast<uint32_t>(width), static_cast<uint32_t>(height)))
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
            .setPreTransform(capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(vk::True)
            .setOldSwapchain(oldSwapchain);

        vk::SharedSwapchainKHR swapchain(
            device.get().createSwapchainKHR(swapchainCreationInfo),
            device,
            platformSurface);

        // Retrieve swapchain images
        std::vector<vk::SharedImage> sharedImages =
            device.get().getSwapchainImagesKHR(swapchain.get())
            | std::views::transform([&device](const vk::Image& img) {
                return vk::SharedImage(img, device, vk::SwapchainOwns::yes);
            })
            | std::ranges::to<std::vector<vk::SharedImage>>();

        // Create backbuffers and framebuffers
        std::vector<nvrhi::TextureHandle> backBuffers;
        std::vector<nvrhi::FramebufferHandle> framebuffers;
        backBuffers.reserve(sharedImages.size());
        framebuffers.reserve(sharedImages.size());

        for (const auto& img : sharedImages) {
            auto textureDesc = nvrhi::TextureDesc()
                .setDimension(nvrhi::TextureDimension::Texture2D)
                .setFormat(nvrhi::Format::BGRA8_UNORM)
                .setWidth(static_cast<uint32_t>(width))
                .setHeight(static_cast<uint32_t>(height))
                .setIsRenderTarget(true)
                .setDebugName("BackBuffer")
                .setInitialState(nvrhi::ResourceStates::Present)
                .setKeepInitialState(true);

            nvrhi::TextureHandle handle = nvrhiDevice->createHandleForNativeTexture(
                nvrhi::ObjectTypes::VK_Image,
                nvrhi::Object(img.get()),
                textureDesc
            );
            backBuffers.push_back(handle);

            auto framebufferDesc = nvrhi::FramebufferDesc().addColorAttachment(handle);
            nvrhi::FramebufferHandle framebuffer = nvrhiDevice->createFramebuffer(framebufferDesc);
            framebuffers.push_back(framebuffer);
        }

        // Create render complete semaphores (one per image)
        std::vector<vk::SharedSemaphore> renderCompleteSemaphores;
        renderCompleteSemaphores.reserve(backBuffers.size());
        vk::SemaphoreCreateInfo semaphoreInfo;
        for (size_t i = 0; i < backBuffers.size(); ++i) {
            vk::Semaphore renderSem = device.get().createSemaphore(semaphoreInfo);
            renderCompleteSemaphores.push_back(vk::SharedSemaphore(renderSem, device));
        }

        PlatformSwapchain result;
        result.mSwapchain = std::move(swapchain);
        result.mDevice = device;  // Save device reference for acquire/present operations
        result.mSwapchainImages = std::move(sharedImages);
        result.mBackBuffers = std::move(backBuffers);
        result.mFramebuffers = std::move(framebuffers);
        result.mRenderCompleteSemaphores = std::move(renderCompleteSemaphores);
        result.mWidth = static_cast<uint32_t>(width);
        result.mHeight = static_cast<uint32_t>(height);
        result.mFormat = nvrhi::Format::BGRA8_UNORM;
        result.mCurrentImageIndex = UINT32_MAX;

        return result;
    }
}
