module ImGui.ImGuiApplication;

import Core.Application;
import "imgui.h";
import "backends/imgui_impl_vulkan.h";
import "SDL3/SDL.h";
import Render.Color;
import Core.Events;

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
        init_info.DescriptorPool = mImGuiDescriptorPool.get();
        init_info.DescriptorPoolSize = 0;
        // MinImageCount should be the minimum swapchain images
        init_info.MinImageCount = static_cast<uint32_t>(mSwapchainData.swapchainImages.size());
        // ImageCount should be MaxFramesInFlight to match the number of in-flight frames
        // This ensures ImGui keeps enough buffer versions to avoid destroying buffers still in use
        init_info.ImageCount = MaxFramesInFlight;
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

        // Load Vulkan functions using DEVICE proc addr for device-level functions
        // vkCmdBeginRenderingKHR is a device-level function, not instance-level
        VkDevice vkDevice = mVkDevice.get();
        ImGui_ImplVulkan_LoadFunctions(vk::ApiVersion12, [](const char *function_name, void *user_data) {
            VkDevice device = *static_cast<VkDevice *>(user_data);
            // First try device-level functions (for commands like vkCmdBeginRenderingKHR)
            PFN_vkVoidFunction func = vkGetDeviceProcAddr(device, function_name);
            return func;
        }, &vkDevice);

        ImGui_ImplVulkan_Init(&init_info);

        // Init sampler
        nvrhi::SamplerDesc samplerDesc{};
        mImGuiTextureSampler = mNvrhiDevice->createSampler(samplerDesc);
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

        mImGuiTextureSampler.Reset();
        mImGuiDescriptorPool.reset();

        Application::Destroy();
    }

    void ImGuiApplication::OnUpdate(std::chrono::duration<float> deltaTime) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Application::OnUpdate(deltaTime);

        ImGui::Render();

        OnFrameEnded([this] {
            auto &io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        });
    }

    void ImGuiApplication::OnRender(const nvrhi::CommandListHandle &command_list,
                                                 const nvrhi::FramebufferHandle &framebuffer) {
        Application::OnRender(command_list, framebuffer);

        nvrhi::TextureSubresourceSet allSubresources(0, nvrhi::TextureSubresourceSet::AllMipLevels,
                                                     0, nvrhi::TextureSubresourceSet::AllArraySlices);

        command_list->setResourceStatesForFramebuffer(framebuffer);
        command_list->commitBarriers();

        vk::CommandBuffer vkCmdBuf{command_list->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)};

        vk::RenderingAttachmentInfoKHR colorAttachment{};
        colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfoKHR;
        colorAttachment.imageView = static_cast<VkImageView>(
            framebuffer->getDesc().colorAttachments[0].texture->getNativeView(
                nvrhi::ObjectTypes::VK_ImageView, nvrhi::Format::UNKNOWN, allSubresources
            )
        );
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        vk::RenderingInfoKHR renderingInfo{};
        renderingInfo.sType = vk::StructureType::eRenderingInfoKHR;
        renderingInfo.renderArea = vk::Rect2D{{0, 0}, {mSwapchainData.width, mSwapchainData.height}};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBuf.beginRenderingKHR(&renderingInfo);

        // Render ImGui
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuf, VK_NULL_HANDLE);

        vkCmdBuf.endRenderingKHR();
    }

    void ImGuiApplication::OnEvent(const Event &event) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        Application::OnEvent(event);
    }

    void ImGuiApplication::DetachAllLayers() {
        mNvrhiDevice->waitForIdle();

        Application::DetachAllLayers();
    }
}
