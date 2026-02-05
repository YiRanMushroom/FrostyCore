export module Core.Application;

import Core.Prelude;
import Vendor.ApplicationAPI;
import Render.Color;
import Core.Layer;
import Core.Events;
import Render.Swapchain;
import Core.Utilities;
import "SDL3/SDL.h";
import "SDL3/SDL_video.h";

import Render.CommandListSubmissionContext;

namespace
Engine {
    export class Application;

    export class Layer : public RefCounted {
    public:
        virtual ~Layer() = default;

        virtual void OnAttach(const Ref<Application> &app);

        virtual void OnUpdate(std::chrono::duration<float> deltaTime) {}

        virtual bool OnEvent(const Event &event) {
            return false;
        }

        virtual void OnRender(const nvrhi::CommandListHandle &commandList,
                              const nvrhi::FramebufferHandle &framebuffer,
                              uint32_t frameIndex) {}

        virtual void OnFrameEnded(std::function<void()> callback);

        template<typename T>
        std::future<T> SendToMainThreadToExecute(std::function<T()> func);

        virtual void OnDetach();

    protected:
        Weak<Application> mApp{};
    };
}


namespace
Engine {
    // Simple message callback for NVRHI
    class NvrhiMessageCallback;

    export struct WindowCreationInfo {
        const char *Title = "NVRHI Vulkan Application";
        int Width = 1280;
        int Height = 720;
        uint32_t SDLWindowFlags = SDL_WINDOW_RESIZABLE;
    };

    // Application class with all inline implementations
    class Application : public Engine::RefCounted {
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

    private:
        [[nodiscard]] const nvrhi::CommandListHandle &GetCommandList() const { return mCommandList; }

    public:
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

        template<typename T>
        std::future<T> SendToMainThreadToExecute(std::function<T()> func) {
            auto promise = std::make_shared<std::promise<T>>();
            auto future = promise->get_future();

            OnFrameEnded([func = std::move(func), promise]() {
                try {
                    if constexpr (std::is_void_v<T>) {
                        func();
                        promise->set_value();
                    } else {
                        T result = func();
                        promise->set_value(std::move(result));
                    }
                } catch (...) {
                    try {
                        promise->set_exception(std::current_exception());
                    } catch (...) {
                        // set_exception() may throw too
                    }
                }
            });

            return future;
        }

        const Ref<CommandListSubmissionContext> &GetCommandListSubmissionContext() const {
            return mCommandListSubmissionContext;
        }

    protected:
        [[nodiscard]] virtual nvrhi::Color GetClearColor() const {
            return {
                Color::MyBlue.r / 255.0f,
                Color::MyBlue.g / 255.0f,
                Color::MyBlue.b / 255.0f,
                Color::MyBlue.a / 255.0f
            };
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
        static constexpr uint32_t MaxFrameInFlight = 5;

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
        std::array<nvrhi::EventQueryHandle, MaxFrameInFlight> mRenderCompleteEvents;
        uint32_t mCurrentFrameIndex = 0;

        // probably you should never use this
        uint32_t mCurrentImageIndex = 0;

        // State
        bool mRunning = false;
        bool mNeedsResize = false;
        bool mMinimized = false;

        // time
        std::chrono::steady_clock::time_point mLastFrameTimestamp;

        std::chrono::duration<float> mGCTimeCounter{};

        std::vector<Ref<Layer>> mLayers;

        // tasks to execute
        std::vector<std::function<void()>> mDeferredTasks;

        Ref<CommandListSubmissionContext> mCommandListSubmissionContext;

    public:
        void PushLayer(const Ref<Layer> &layer);

        Ref<Layer> PopLayer(Weak<Layer> layer) {
            if (auto locked = layer.Lock()) {
                std::erase(mLayers, locked);
                locked->OnDetach();
                return locked;
            }
            return nullptr;
        }

        template<typename T, typename... Args>
        Ref<T> EmplaceLayer(Args &&... args) {
            static_assert(std::is_base_of_v<Layer, T>, "T must be derived from Layer");
            auto layer = Engine::MakeRef<T>(std::forward<Args>(args)...);
            PushLayer(layer);
            return layer;
        }

        void TransitionToLayer(Ref<Layer> oldLayer, Ref<Layer> newLayer);

        virtual void DetachAllLayers() {
            for (auto &layer: mLayers) {
                layer->OnDetach();
                std::move(layer).Destroy();
            }
        }

        void SubmitToCommandListThread(std::move_only_function<void()> task) {
            if (mCommandListSubmissionContext) {
                mCommandListSubmissionContext->SubmitTaskImmediate(std::move(task));
            } else {
                throw Engine::RuntimeException("CommandListSubmissionThread is not initialized");
            }
        }
    };

    template<typename T>
    std::future<T> Layer::SendToMainThreadToExecute(std::function<T()> func) {
        if (auto app = mApp.Lock()) {
            return app->SendToMainThreadToExecute<T>(std::move(func));
        }
        throw Engine::RuntimeException("Layer is not attached to an Application");
    }
}
