module ImGui.ImGuiApplication;

import Core.Application;
import "imgui.h";
import "backends/imgui_impl_vulkan.h";
import "SDL3/SDL.h";

namespace
Engine {
    void ImGuiApplication::Init(WindowCreationInfo info) {
        Application::Init(info);

        // Create Descriptor Pool for ImGui
        VkDescriptorPoolSize pool_sizes[] =
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize &pool_size: pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t) IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        mImGuiDescriptorPool = vk::SharedDescriptorPool(
            mVkDevice.get().createDescriptorPool(pool_info),
            mVkDevice
        );

        float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

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
        init_info.DescriptorPool = mImGuiDescriptorPool.get();
        init_info.DescriptorPoolSize = 0;
        init_info.MinImageCount = static_cast<uint32_t>(mSwapchainData.swapchainImages.size());
        init_info.ImageCount = static_cast<uint32_t>(mSwapchainData.swapchainImages.size());
        init_info.Allocator = nullptr;
        init_info.UseDynamicRendering = true;
        // init_info.PipelineInfoMain.RenderPass = mImGuiRenderPass.get();
        // init_info.PipelineInfoMain.Subpass = 0;
        // init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = nullptr;

        VkFormat colorFormat = static_cast<VkFormat>(mSwapchainData.format);
        VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

        init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;

        VkInstance vkInstance = mVkInstance.get();

        ImGui_ImplVulkan_LoadFunctions(vk::ApiVersion12, [](const char *function_name, void *vulkan_instance) {
            return vkGetInstanceProcAddr(*static_cast<VkInstance *>(vulkan_instance), function_name);
        }, &vkInstance);

        ImGui_ImplVulkan_Init(&init_info);

        // int w, h;
        // SDL_GetWindowSize(mWindow.get(), &w, &h);
        //
        // mImGuiWindowData.Surface = mVkSurface.get();
        // mImGuiWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        //     mVkPhysicalDevice.get(),
        //     mVkSurface.get(),
        //     nullptr,
        //     0,
        //     VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        // );
        // mImGuiWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        //     mVkPhysicalDevice.get(),
        //     mVkSurface.get(),
        //     nullptr,
        //     0
        // );
        //
        // uint32_t queueFamilyIndex = ImGui_ImplVulkanH_SelectQueueFamilyIndex(mVkPhysicalDevice.get());
        //
        // ImGui_ImplVulkanH_CreateOrResizeWindow(
        //     mVkInstance.get(),
        //     mVkPhysicalDevice.get(),
        //     mVkDevice.get(),
        //     &mImGuiWindowData,
        //     queueFamilyIndex,
        //     nullptr,
        //     w,
        //     h,
        //     static_cast<uint32_t>(mSwapchainData.swapchainImages.size()),
        //     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        // );
    }

    void ImGuiApplication::Destroy() {
        // Wait for device to be idle before destroying ImGui resources
        // This ensures all buffers used by ImGui are no longer in use by the GPU
        if (mVkDevice) {
            mVkDevice.get().waitIdle();
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        mImGuiDescriptorPool.reset();

        Application::Destroy();
    }

    void ImGuiApplication::OnUpdate(std::chrono::duration<float> deltaTime) {
        Application::OnUpdate(deltaTime);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        static bool show_demo_window = true;
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();

        OnFrameEnded([this] {
            auto &io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        });
    }

    void ImGuiApplication::OnCommandListRecorded(const nvrhi::CommandListHandle &command_list,
                                                 const nvrhi::FramebufferHandle &framebuffer) {
        Application::OnCommandListRecorded(command_list, framebuffer);

        // Get native Vulkan command buffer - render pass already started by Application
        VkCommandBuffer vkCmdBuf = static_cast<VkCommandBuffer>(
            command_list->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)
        );

        // Render ImGui (pipeline is handled internally by ImGui)
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuf, VK_NULL_HANDLE);
    }

    void ImGuiApplication::OnEvent(const SDL_Event &event) {
        Application::OnEvent(event);
        ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            // ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(mSwapchainData.swapchainImages.size()));
            //
            // uint32_t queueFamilyIndex = ImGui_ImplVulkanH_SelectQueueFamilyIndex(mVkPhysicalDevice.get());
            //
            // ImGui_ImplVulkanH_CreateOrResizeWindow(
            //     mVkInstance.get(),
            //     mVkPhysicalDevice.get(),
            //     mVkDevice.get(),
            //     &mImGuiWindowData,
            //     queueFamilyIndex,
            //     nullptr,
            //     static_cast<int>(event.window.data1),
            //     static_cast<int>(event.window.data2),
            //     static_cast<uint32_t>(mSwapchainData.swapchainImages.size()),
            //     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            // );
        }
    }

    void ImGuiApplication::CreateImGuiRenderPass() {
        vk::AttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = mSwapchainData.format;
        colorAttachmentDesc.samples = vk::SampleCountFlagBits::e1;
        colorAttachmentDesc.loadOp = vk::AttachmentLoadOp::eLoad;
        colorAttachmentDesc.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachmentDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachmentDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachmentDesc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachmentDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpassDesc{};
        subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;

        vk::SubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo renderPassInfo{};
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachmentDesc;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDesc;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        mImGuiRenderPass = vk::SharedRenderPass(
            mVkDevice.get().createRenderPass(renderPassInfo),
            mVkDevice
        );
    }
}
