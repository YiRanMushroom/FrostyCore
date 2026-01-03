export module ImGui.ImGui;

export import "imgui.h";
export import "imgui_internal.h";
export import "backends/imgui_impl_sdl3.h";
export import "backends/imgui_impl_vulkan.h";
import Vendor.GraphicsAPI;

namespace ImGui {
    export inline void StyleColorHazel() {
        auto &colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

        // Headers
        colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Buttons
        colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Frame BG
        colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Tabs
        // colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        // colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.185f, 0.19f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.285f, 0.29f, 1.00f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.22f, 0.225f, 0.23f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.35f, 0.40f, 0.60f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.13f, 0.135f, 0.14f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.18f, 0.185f, 0.19f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.25f, 0.30f, 0.45f, 1.00f);

        // Title
        colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Resize Grip
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

        // Check Mark
        colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);

        // Slider
        colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);

        // Docking
        // colors[ImGuiCol_DockingPreview] = ImVec4(0.31f, 0.31f, 0.31f, 0.7f);
        // colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    }

    export class ImGuiImage;

    export class IImGuiImageHolder : public nvrhi::IResource {
    public:
        virtual ~IImGuiImageHolder() override {
            IImGuiImageHolder::Destroy();
        }

        friend class ImGuiImage;

        virtual void Destroy() {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(mImGuiTextureID));
            mImGuiTextureID = 0;
        }

        virtual void Init(nvrhi::ITexture *texture, nvrhi::ISampler *sampler) {
            if (mImGuiTextureID != 0) {
                Destroy();
            }

            // 1. Get native Vulkan objects from NVRHI
            VkImageView imageView = texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
            VkSampler vkSampler = sampler->getNativeObject(nvrhi::ObjectTypes::VK_Sampler);

            // 2. Register with ImGui's Vulkan backend
            // This creates a VkDescriptorSet internally
            mImGuiTextureID = reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(
                vkSampler,
                imageView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ));
        }

        ImTextureID GetImGuiTextureID() const {
            return mImGuiTextureID;
        }

    private:
        ImTextureID mImGuiTextureID{};
    };

    export class ImGuiImageHolder : public nvrhi::RefCounter<IImGuiImageHolder> {
    public:
        ImGuiImageHolder(nvrhi::ITexture *texture, nvrhi::ISampler *sampler)
            : nvrhi::RefCounter<IImGuiImageHolder>() {
            IImGuiImageHolder::Init(texture, sampler);
        }
    };

    class ImGuiImage {
    public:
        ImGuiImage(nvrhi::TextureHandle texture, nvrhi::SamplerHandle sampler)
            : mTexture(texture), mSampler(sampler) {
            mHolder = new ImGuiImageHolder(mTexture.Get(), mSampler.Get());
            mHolder->Release();
        }

        ImTextureID GetImGuiTextureID() const {
            return mHolder->GetImGuiTextureID();
        }

        void Reset() {
            mHolder.Reset();
            mTexture.Reset();
            mSampler.Reset();
        }

        ~ImGuiImage() {
            Reset();
        }

        ImGuiImage() = default;

    private:
        nvrhi::SamplerHandle mSampler;
        nvrhi::TextureHandle mTexture;
        nvrhi::RefCountPtr<IImGuiImageHolder> mHolder;

    public:
        static ImGuiImage Create(nvrhi::TextureHandle texture, nvrhi::SamplerHandle sampler) {
            return ImGuiImage(texture, sampler);
        }
    };
}
