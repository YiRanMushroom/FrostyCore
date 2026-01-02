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

    public:
        vk::SharedDescriptorPool mImGuiDescriptorPool;

        ImGui_ImplVulkanH_Window mImGuiWindowData;

    protected:
        void CreateImGuiRenderPass();
    };
}
