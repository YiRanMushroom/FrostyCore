export module Core.Application;

import Core.Prelude;
import Vendor.ApplicationAPI;
import Render.Color;
import Core.Layer;
import Core.Events;
import Render.Swapchain;
import "SDL3/SDL.h";
import "SDL3/SDL_video.h";

namespace
Engine {
    export class Application;

    export class Layer {
    public:
        virtual ~Layer() = default;

        virtual void OnAttach(const std::shared_ptr<Application> &app);

        virtual void OnUpdate(std::chrono::duration<float> deltaTime) {}

        virtual bool OnEvent(const Event &event) {
            return false;
        }

        virtual void OnRender(const nvrhi::CommandListHandle &commandList,
                              const nvrhi::FramebufferHandle &framebuffer) {}

        virtual void OnFrameEnded(std::function<void()> callback);

        virtual void OnDetach();

    protected:
        std::shared_ptr<Application> mApp{};
    };
}


namespace
Engine {
    // Simple message callback for NVRHI
    class NvrhiMessageCallback : public nvrhi::IMessageCallback {
    public:
        void message(nvrhi::MessageSeverity severity, const char *messageText) override;
    };


    export struct WindowCreationInfo {
        const char *Title = "NVRHI Vulkan Application";
        int Width = 1280;
        int Height = 720;
        uint32_t SDLWindowFlags = SDL_WINDOW_RESIZABLE;
    };

    // Application class with all inline implementations
    class Application : public std::enable_shared_from_this<Application> {
    public:
        constexpr static size_t MaxFramesInFlight = 3;

        Application() = default;

        virtual ~Application() = default;

        // Non-copyable
        Application(const Application &) = delete;

        Application &operator=(const Application &) = delete;

        // Getter methods
        [[nodiscard]] const std::shared_ptr<SDL_Window> &GetWindow() const { return mWindow; }

        [[nodiscard]] const vk::SharedInstance &GetVkInstance() const { return mVkInstance; }
        [[nodiscard]] const vk::SharedPhysicalDevice &GetVkPhysicalDevice() const { return mVkPhysicalDevice; }
        [[nodiscard]] const vk::SharedSurfaceKHR &GetVkSurface() const { return mVkSurface; }
        [[nodiscard]] const vk::SharedDevice &GetVkDevice() const { return mVkDevice; }
        [[nodiscard]] const vk::SharedQueue &GetVkQueue() const { return mVkQueue; }

        [[nodiscard]] const nvrhi::vulkan::DeviceHandle &GetNvrhiDevice() const { return mNvrhiDevice; }
        [[nodiscard]] const nvrhi::CommandListHandle &GetCommandList() const { return mCommandList; }

        [[nodiscard]] const PlatformSwapchain &GetSwapchain() const { return mSwapchain; }

        // Legacy compatibility - maps to new Swapchain API
        [[nodiscard]] const PlatformSwapchain &GetSwapchainData() const { return mSwapchain; }

        [[nodiscard]] bool IsRunning() const { return mRunning; }
        [[nodiscard]] bool IsMinimized() const { return mMinimized; }

        virtual void Init(WindowCreationInfo info = {});

        virtual void OnEvent(const Event &event);

        virtual void OnUpdate(std::chrono::duration<float> deltaTime);

        virtual void Run();

        virtual void Destroy();

        virtual void OnFrameEnded(std::function<void()> callback);

    protected:
        [[nodiscard]] virtual nvrhi::Color GetClearColor() const {
            return Color::MyBlue;
        }

        virtual void CreateWindow(WindowCreationInfo info);

        void InitVulkan();

        void CreateVulkanInstance();

        void SetupDebugMessenger();

        void SelectPhysicalDevice();

        void CreateSurface();

        void CreateLogicalDevice();

        void InitNVRHI();

        void CreateSwapchain();

        void CreateSyncObjects();

        void RecreateSwapchain();


        void ExecuteDeferredTasks();

        void ProcessEvents();

    public:
        virtual void OnPostRender();

    protected:
        virtual void RenderFrame();

        virtual void OnRender(const nvrhi::CommandListHandle &,
                              const nvrhi::FramebufferHandle &);

    protected:
        // Member variables (order matters for destruction)
        static constexpr uint32_t MaxFrameInFlight = 3;

        // Window
        std::shared_ptr<SDL_Window> mWindow;

        // Vulkan objects
        vk::SharedInstance mVkInstance;
        vk::DebugUtilsMessengerEXT mDebugMessenger; // Debug messenger for validation
        vk::SharedPhysicalDevice mVkPhysicalDevice;
        vk::SharedSurfaceKHR mVkSurface;
        vk::SharedDevice mVkDevice;
        vk::SharedQueue mVkQueue;

        // NVRHI
        std::shared_ptr<NvrhiMessageCallback> mMessageCallback;
        nvrhi::vulkan::DeviceHandle mNvrhiDevice;
        nvrhi::CommandListHandle mCommandList;

        // Swapchain (uses new PlatformSwapchain class)
        PlatformSwapchain mSwapchain;

        // Frame-in-flight synchronization (separate from swapchain)
        std::vector<vk::SharedSemaphore> mAcquireSemaphores; // Per-frame (for acquire)
        std::array<vk::SharedFence, MaxFrameInFlight> mRenderCompleteFences;
        uint32_t mCurrentFrame = 0;

        // State
        bool mRunning = false;
        bool mNeedsResize = false;
        bool mMinimized = false;

        // time
        std::chrono::steady_clock::time_point mLastFrameTimestamp;

        std::chrono::duration<float> mGCTimeCounter{};

        std::vector<std::shared_ptr<Layer>> mLayers;

        // tasks to execute
        std::vector<std::function<void()>> mDeferredTasks;

    public:
        void PushLayer(const std::shared_ptr<Layer> &layer) {
            mLayers.push_back(layer);
            layer->OnAttach(shared_from_this());
        }

        std::shared_ptr<Layer> PopLayer(std::weak_ptr<Layer> layer) {
            if (auto locked = layer.lock()) {
                std::erase(mLayers, locked);
                locked->OnDetach();
                return locked;
            }
            return nullptr;
        }

        template<typename T, typename... Args>
        std::shared_ptr<T> EmplaceLayer(Args &&... args) {
            static_assert(std::is_base_of_v<Layer, T>, "T must be derived from Layer");
            auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            PushLayer(layer);
            return layer;
        }

        void TransitionToLayer(std::shared_ptr<Layer> oldLayer, std::shared_ptr<Layer> newLayer) {
            for (auto &layer: mLayers) {
                if (layer == oldLayer) {
                    std::swap(layer, newLayer);
                    oldLayer->OnDetach();
                    newLayer->OnAttach(shared_from_this());
                    return;
                }
            }

            throw Engine::RuntimeException("Old layer not found in layer stack");
        }

        virtual void DetachAllLayers() {
            for (auto &layer: mLayers) {
                layer->OnDetach();
            }

            mLayers.clear();
        }
    };
}
