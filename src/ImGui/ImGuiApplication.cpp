module ImGui.ImGuiApplication;

import Core.Application;
import "imgui.h";
import "backends/imgui_impl_vulkan.h";
import "SDL3/SDL.h";
import Render.Color;
import Core.Events;
import Render.Utilities;
import "vendor/nvrhi/src/vulkan/vulkan-backend.h";
import Vendor.GraphicsAPI;

namespace
Engine {
    void ImGuiApplication::Init(WindowCreationInfo info) {
        Application::Init(info);


        float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
        // Enable Docking and Multi-Viewport

        // Setup Dear ImGui style
        ImGui::StyleColorHazel();
        //ImGui::StyleColorsLight();

        // Setup scaling
        ImGuiStyle &style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
        style.FontScaleDpi = main_scale;
        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForVulkan(mWindow.get());
        ImGui_ImplVulkan_InitInfo init_info = {};

        // CreateImGuiRenderPass();

        init_info.Instance = mVkInstance.get();
        init_info.PhysicalDevice = mVkPhysicalDevice.get();
        init_info.Device = mVkDevice.get();
        init_info.QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(mVkPhysicalDevice.get());
        init_info.Queue = mVkQueue.get();
        init_info.PipelineCache = nullptr;
        init_info.DescriptorPool = VK_NULL_HANDLE;  // No longer needed - ImGui manages internally
        init_info.DescriptorPoolSize = 0;           // No longer needed - ImGui manages internally
        // MinImageCount should be the minimum swapchain images
        init_info.MinImageCount = mSwapchain.GetImageCount();
        // ImageCount should be MaxFramesInFlight to match the number of in-flight frames
        // This ensures ImGui keeps enough buffer versions to avoid destroying buffers still in use
        init_info.ImageCount = MaxFramesInFlight;
        init_info.Allocator = nullptr;
        init_info.UseDynamicRendering = true;
        // init_info.PipelineInfoMain.RenderPass = mImGuiRenderPass.get();
        // init_info.PipelineInfoMain.Subpass = 0;
        // init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = nullptr;

        VkFormat colorFormat = static_cast<VkFormat>(mSwapchain.GetVkFormat());
        VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

        init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;

        // Load Vulkan functions using DEVICE proc addr for device-level functions
        // vkCmdBeginRenderingKHR is a device-level function, not instance-level
        VkDevice vkDevice = mVkDevice.get();
        ImGui_ImplVulkan_LoadFunctions(vk::ApiVersion12, [](const char *function_name, void *user_data) {
            VkDevice device = *static_cast<VkDevice *>(user_data);
            // First try device-level functions (for commands like vkCmdBeginRenderingKHR)
            PFN_vkVoidFunction func = vkGetDeviceProcAddr(device, function_name);
            return func;
        }, &vkDevice);

        // Pass nvrhi device handle so ImGui can dynamically get the queue mutex
        init_info.NvrhiDeviceHandle = mNvrhiDevice.Get();

        ImGui_ImplVulkan_Init(&init_info);

        // Init sampler
        nvrhi::SamplerDesc samplerDesc{};
        mImGuiTextureSampler = mNvrhiDevice->createSampler(samplerDesc);
    }

    void ImGuiApplication::Destroy() {
        ImGui::RunGarbageCollectionAllFrames();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        mImGuiTextureSampler.Reset();

        Application::Destroy();
    }

    void ImGuiApplication::OnUpdate(std::chrono::duration<float> deltaTime) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Application::OnUpdate(deltaTime);

        ImGui::Render();
    }

    void ImGuiApplication::OnRender(const nvrhi::CommandListHandle &command_list,
                                    const nvrhi::FramebufferHandle &framebuffer) {
        Application::OnRender(command_list, framebuffer);
        ImGui::RunGarbageCollection(mCurrentFrame);

        command_list->clearState();

        Render::SimpleRenderingGuard renderingGuard(command_list, framebuffer,
                                                    framebuffer->getDesc().colorAttachments[0].texture->getDesc().width,
                                                    framebuffer->getDesc().colorAttachments[0].texture->getDesc().height);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), renderingGuard.GetVkCommandBuffer(), VK_NULL_HANDLE);
    }

    void ImGuiApplication::OnEvent(const Event &event) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        Application::OnEvent(event);
    }

    void ImGuiApplication::OnPostRender() {
        auto &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        Application::OnPostRender();
    }

    void ImGuiApplication::DetachAllLayers() {
        Application::DetachAllLayers();
    }
}
