export module ImGui.ImGuiApplication;

import Core.Application;
import ImGui.ImGui;
import Core.Events;

namespace Engine {
    export class ImGuiApplication : public Application {
    public:
        virtual void Init(WindowCreationInfo info) override;
        virtual void Destroy() override;
        virtual void OnUpdate(std::chrono::duration<float> deltaTime) override;
        virtual void OnRender(const nvrhi::CommandListHandle &, const nvrhi::FramebufferHandle &) override;
        virtual void OnEvent(const Event &event) override;
        virtual void OnPostRender() override;

        virtual const nvrhi::SamplerHandle &GetImGuiTextureSampler() const {
            return mImGuiTextureSampler;
        }

        virtual void DetachAllLayers() override;

    protected:
        nvrhi::SamplerHandle mImGuiTextureSampler;

        ImGui_ImplVulkanH_Window mImGuiWindowData;

    protected:
        void CreateImGuiRenderPass();

        // target 100 FPS when minimized
        std::chrono::duration<float, std::milli> mTargetFrameTimeWhenMinimized{10.f};
    };
}
