export module ImGui.ImGuiApplication;

import Core.Application;
import ImGui.ImGui;

namespace Engine {
    export class ImGuiApplication : public Application {
    public:
        virtual void Init(WindowCreationInfo info) override;
        virtual void Destroy() override;
        virtual void OnUpdate(std::chrono::duration<float> deltaTime) override;
        virtual void OnCommandListRecorded(const nvrhi::CommandListHandle &, const nvrhi::FramebufferHandle &) override;
        virtual void OnEvent(const SDL_Event &event) override;

    protected:
        vk::SharedDescriptorPool mImGuiDescriptorPool;
        nvrhi::SamplerHandle mImGuiTextureSampler;

        ImGui_ImplVulkanH_Window mImGuiWindowData;

        nvrhi::TextureHandle mMyTexture;
        ImTextureID mImGuiTexture{};

        static ImTextureID GetImGuiTextureID(nvrhi::ITexture* texture, nvrhi::ISampler* sampler) {
            // 1. Get native Vulkan objects from NVRHI
            VkImageView imageView = texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
            VkSampler vkSampler = sampler->getNativeObject(nvrhi::ObjectTypes::VK_Sampler);

            // 2. Register with ImGui's Vulkan backend
            // This creates a VkDescriptorSet internally
            return reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(
                vkSampler,
                imageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ));
        }

        static void RemoveImGuiTextureID(ImTextureID imGuiTextureID) {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(imGuiTextureID));
        }

        void InitMyTexture();

    protected:
        void CreateImGuiRenderPass();
    };
}
