export module Render.Utilities;
import Vendor.GraphicsAPI;

namespace
Engine::Render {
    export class SimpleRenderingGuard {
    public:
        SimpleRenderingGuard(const SimpleRenderingGuard &) = delete;

        SimpleRenderingGuard &operator=(const SimpleRenderingGuard &) = delete;

        SimpleRenderingGuard(SimpleRenderingGuard &&) = delete;

        SimpleRenderingGuard &operator=(SimpleRenderingGuard &&) = delete;

        SimpleRenderingGuard(const nvrhi::CommandListHandle& command_list,
                             const nvrhi::FramebufferHandle &framebuffer,
                             uint32_t width,
                             uint32_t height)
            : mCommandList(command_list) {
            nvrhi::TextureSubresourceSet allSubresources(0, nvrhi::TextureSubresourceSet::AllMipLevels,
                                                         0, nvrhi::TextureSubresourceSet::AllArraySlices);

            vk::CommandBuffer vkCmdBuf{mCommandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)};

            vk::RenderingAttachmentInfoKHR colorAttachment{};
            colorAttachment.sType = vk::StructureType::eRenderingAttachmentInfoKHR;
            colorAttachment.imageView = framebuffer->getDesc().colorAttachments[0]
                    .texture->getNativeView(nvrhi::ObjectTypes::VK_ImageView,
                                            nvrhi::Format::UNKNOWN, allSubresources);
            colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
            colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

            vk::RenderingInfoKHR renderingInfo{};
            renderingInfo.sType = vk::StructureType::eRenderingInfoKHR;
            renderingInfo.renderArea = vk::Rect2D{{0, 0}, {width, height}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBuf.beginRenderingKHR(&renderingInfo);
        }

        [[nodiscard]] vk::CommandBuffer GetVkCommandBuffer() const {
            return vk::CommandBuffer{mCommandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)};
        }

        ~SimpleRenderingGuard() {
            vk::CommandBuffer vkCmdBuf{mCommandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)};
            vkCmdBuf.endRenderingKHR();
        }

    private:
        const nvrhi::CommandListHandle& mCommandList;
    };
}
