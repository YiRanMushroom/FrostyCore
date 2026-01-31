export module Render.Renderer2D:Renderer;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;
import Render.VirtualTextureManager;
export import Render.Transform;
import glm;
import Core.Utilities;
import Core.Coroutine;

import Render.CommandListSubmissionContext;
import Render.RenderAPI;

import :ForwardDecleration;
import :Misc;
import :TriangleAndQuad;
import :Line;
import :Eclipse;
import :DrawCommands;
import :ClipRegionManager;

namespace
Engine {
    export struct Renderer2DDescriptor {
        glm::u32vec2 OutputSize;
        // float VirtualSizeWidth;
        mutable std::vector<Ref<ITransform>> Transforms;
    };

    export struct Renderer2DBeginRenderingInfo {
        // nvrhi::Color ClearColor = nvrhi::Color(0, 0, 0, 0);
        glm::u8vec4 ClearColor = glm::u8vec4(0, 0, 0, 0);
    };

    export class IRenderer2D : public RefCounted {
    public:
        virtual void BeginRendering(const Renderer2DBeginRenderingInfo &info = {}) = 0;

        [[nodiscard]] virtual const nvrhi::CommandListHandle &GetCommandList() const = 0;

        virtual void EndRendering() = 0;

        virtual void OnResize(uint32_t width, uint32_t height) = 0;

        [[nodiscard]] virtual nvrhi::ITexture *GetTexture() const = 0;

        virtual void Clear() = 0;

        [[nodiscard]] virtual int GetCurrentDepth() const = 0;

        virtual void SetCurrentDepth(int depth) = 0;

        virtual uint32_t RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle &texture) = 0;

        virtual ClipRegionManager &GetClipRegionManager() = 0;

        virtual Awaitable<uint32_t> GetEntityIDAtPixelPositionAsync(const glm::uvec2 &pixelPosition) = 0;

        virtual RHIAPI GetRHIAPI() = 0;
    };

    export class NVRenderer2D : public IRenderer2D {
        template<StringLiteral CommandName>
        friend class CommandEncoder;

    public:
        NVRenderer2D(const Renderer2DDescriptor &desc,
                     Ref<CommandListSubmissionContext> submissionContext);

        void BeginRendering(const Renderer2DBeginRenderingInfo &info = {}) override;

        [[nodiscard]] const nvrhi::CommandListHandle &GetCommandList() const override;

        void EndRendering() override;

        void OnResize(uint32_t width, uint32_t height) override;

        [[nodiscard]] nvrhi::ITexture *GetTexture() const override;

        void Clear() override;

        [[nodiscard]] int GetCurrentDepth() const override;

        void SetCurrentDepth(int depth) override;

        uint32_t RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle &texture) override;

        ClipRegionManager &GetClipRegionManager() override;

        Awaitable<uint32_t> GetEntityIDAtPixelPositionAsync(const glm::uvec2 &pixelPosition) override;

        RHIAPI GetRHIAPI() override { return RHIAPI::NVRHI; }

    private:
        void CreateResources();

        void CreatePipelineResources();

        void CreateTriangleBatchRenderingResources(size_t count);

        void CreateLineBatchRenderingResources(size_t count);

        void CreateEllipseBatchRenderingResources(size_t count);

        void CreatePipelines();

        void CreateConstantBuffers();

        void CreatePipelineTriangle();

        void CreatePipelineLine();

        void CreatePipelineEllipse();

        void SubmitTriangleBatchRendering();

        void SubmitLineBatchRendering();

        void SubmitEllipseBatchRendering();

        void Submit();

        glm::mat4 GetViewProjectionMatrix();

        void SetTransforms(std::vector<Ref<ITransform>> transforms);

        nvrhi::DeviceHandle mDevice;
        glm::u32vec2 mOutputSize;

        Ref<CommandListSubmissionContext> mSubmissionContext;

        std::vector<Ref<ITransform>> mTransforms;

        nvrhi::TextureHandle mTexture;

        nvrhi::TextureHandle mTargetIDTexture;

        nvrhi::FramebufferHandle mFramebuffer;

        VirtualTextureManager mVirtualTextureManager;
        ClipRegionManager mClipRegionManager;

        size_t mBindlessTextureArraySizeMax{};
        nvrhi::CommandListHandle mCommandList;
        nvrhi::SamplerHandle mTextureSampler;
        nvrhi::SamplerHandle mFontSampler;

        int mCurrentDepth = 0;

        TriangleRenderingCommandList mTriangleCommandList;
        nvrhi::InputLayoutHandle mTriangleInputLayout;
        nvrhi::GraphicsPipelineHandle mTrianglePipeline;
        nvrhi::BindingLayoutHandle mTriangleBindingLayoutSpace0;
        nvrhi::BindingLayoutHandle mTriangleBindingLayoutSpace1;
        nvrhi::BufferHandle mTriangleConstantBuffer;
        size_t mTriangleBufferInstanceSizeMax;
        std::vector<TriangleBatchRenderingResources> mTriangleBatchRenderingResources;

        LineRenderingCommandList mLineCommandList;
        nvrhi::InputLayoutHandle mLineInputLayout;
        nvrhi::GraphicsPipelineHandle mLinePipeline;
        nvrhi::BindingLayoutHandle mLineBindingLayoutSpace0;
        nvrhi::BufferHandle mLineConstantBuffer;
        size_t mLineBufferVertexSizeMax;
        std::vector<LineBatchRenderingResources> mLineBatchRenderingResources;

        EllipseRenderingCommandList mEllipseCommandList;
        nvrhi::GraphicsPipelineHandle mEllipsePipeline;
        nvrhi::BindingLayoutHandle mEllipseBindingLayoutSpace0;
        nvrhi::BindingLayoutHandle mEllipseBindingLayoutSpace1;
        nvrhi::BufferHandle mEllipseConstantBuffer;
        size_t mEllipseBufferInstanceSizeMax;
        std::vector<EllipseBatchRenderingResources> mEllipseBatchRenderingResources;
    };

    export class VirtualSizeTransform : public RefCounted, public ITransform {
    public:
        void SetVirtualWidth(float virtualWidth) {
            mVirtualSize.x = virtualWidth;
            mCachedTransform.reset();
        }

        void SetVirtualHeight(float virtualHeight) {
            mVirtualSize.y = virtualHeight;
            mCachedTransform.reset();
        }

        virtual void OnFramebufferResized(float newWidth, float newHeight) override {
            mFramebufferSize = {newWidth, newHeight};
            mCachedTransform.reset();
        }

        virtual void DoTransform(glm::mat4 &matrix) override;

    public:
        glm::vec2 mVirtualSize{1920.f, 1080.f};
        glm::vec2 mFramebufferSize{};
        std::optional<glm::mat4> mCachedTransform{};
    };
}
