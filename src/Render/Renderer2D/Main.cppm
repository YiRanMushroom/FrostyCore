export module Render.Renderer2D;

export import :ForwardDecleration;
export import :TriangleAndQuad;
export import :Line;
export import :Misc;
export import :Eclipse;
export import :DrawCommands;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;
import Render.VirtualTextureManager;
import glm;

namespace
Engine {
    export struct Renderer2DDescriptor {
        glm::u32vec2 OutputSize;
        float VirtualSizeWidth;
    };

    export struct Renderer2DBeginRenderingInfo {
        nvrhi::Color ClearColor = nvrhi::Color(0, 0, 0, 0);
    };

    export class Renderer2D {
    public:
        Renderer2D(const Renderer2DDescriptor& desc, nvrhi::DeviceHandle device);

        const glm::vec2& BeginRendering(const Renderer2DBeginRenderingInfo& info = {});

        [[nodiscard]] const nvrhi::CommandListHandle& GetCommandList() const;

        void EndRendering();

        void OnResize(uint32_t width, uint32_t height);

        const glm::vec2& SetVirtualWidth(float virtualWidth);

        [[nodiscard]] nvrhi::ITexture* GetTexture() const;

        void Clear();

        [[nodiscard]] int GetCurrentDepth() const;

        void SetCurrentDepth(int depth);

        uint32_t RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle& texture);

        void DrawTriangleColored(const glm::mat3x2& positions, const glm::u8vec4& color,
                                 std::optional<int> overrideDepth = std::nullopt,
                                 const ClipRegion* clip = nullptr);

        void DrawTriangleTextureVirtual(const glm::mat3x2& positions, const glm::mat3x2& uvs,
                                        uint32_t virtualTextureID, std::optional<int> overrideDepth = std::nullopt,
                                        glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255),
                                        const ClipRegion* clip = nullptr);

        uint32_t DrawTriangleTextureManaged(const glm::mat3x2& positions, const glm::mat3x2& uvs,
                                            const nvrhi::TextureHandle& texture,
                                            std::optional<int> overrideDepth = std::nullopt,
                                            glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255),
                                            const ClipRegion* clip = nullptr);

        void DrawQuadColored(const glm::mat4x2& positions, const glm::u8vec4& color,
                             std::optional<int> overrideDepth = std::nullopt,
                             const ClipRegion* clip = nullptr);

        void DrawQuadTextureVirtual(const glm::mat4x2& positions, const glm::mat4x2& uvs,
                                    uint32_t virtualTextureID, std::optional<int> overrideDepth = std::nullopt,
                                    glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255),
                                    const ClipRegion* clip = nullptr);

        uint32_t DrawQuadTextureManaged(const glm::mat4x2& positions, const glm::mat4x2& uvs,
                                        const nvrhi::TextureHandle& texture,
                                        std::optional<int> overrideDepth = std::nullopt,
                                        glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255),
                                        const ClipRegion* clip = nullptr);

        void DrawQuadFontColoredVirtual(const glm::mat4x2& positions, const glm::mat4x2& uvs,
                                        uint32_t virtualTextureID,
                                        glm::u8vec4 tintColor,
                                        float msdfPixelRange,
                                        std::optional<int> overrideDepth = std::nullopt,
                                        const ClipRegion* clip = nullptr);

        void DrawLine(const glm::vec2& p0, const glm::vec2& p1, const glm::u8vec4& color);

        void DrawCircle(const glm::vec2& center, float radius, const glm::u8vec4& color,
                        std::optional<int> overrideDepth = std::nullopt,
                        const ClipRegion* clip = nullptr);

        void DrawEllipse(const glm::vec2& center, const glm::vec2& radii, float rotation,
                         const glm::u8vec4& color, std::optional<int> overrideDepth = std::nullopt,
                         const ClipRegion* clip = nullptr);

        void DrawRing(const glm::vec2& center, float outerRadius, float innerRadius,
                      const glm::u8vec4& color, std::optional<int> overrideDepth = std::nullopt,
                      const ClipRegion* clip = nullptr);

        void DrawSector(const glm::vec2& center, float radius, float startAngle, float endAngle,
                        const glm::u8vec4& color, std::optional<int> overrideDepth = std::nullopt,
                        const ClipRegion* clip = nullptr);

        void DrawSectorTextureVirtual(const glm::vec2& center, float radius, float startAngle, float endAngle,
                                      uint32_t virtualTextureID,
                                      const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                      std::optional<int> overrideDepth = std::nullopt,
                                      const ClipRegion* clip = nullptr);

        uint32_t DrawSectorTextureManaged(const glm::vec2& center, float radius, float startAngle, float endAngle,
                                          const nvrhi::TextureHandle& texture,
                                          const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                          std::optional<int> overrideDepth = std::nullopt,
                                          const ClipRegion* clip = nullptr);

        void DrawArc(const glm::vec2& center, float radius, float thickness,
                     float startAngle, float endAngle, const glm::u8vec4& color,
                     std::optional<int> overrideDepth = std::nullopt,
                     const ClipRegion* clip = nullptr);

        void DrawEllipseSector(const glm::vec2& center, const glm::vec2& radii, float rotation,
                               float startAngle, float endAngle, const glm::u8vec4& color,
                               std::optional<int> overrideDepth = std::nullopt,
                               const ClipRegion* clip = nullptr);

        void DrawEllipseSectorTextureVirtual(const glm::vec2& center, const glm::vec2& radii, float rotation,
                                             float startAngle, float endAngle, uint32_t virtualTextureID,
                                             const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                             std::optional<int> overrideDepth = std::nullopt,
                                             const ClipRegion* clip = nullptr);

        void DrawEllipseArc(const glm::vec2& center, const glm::vec2& radii, float rotation,
                            float thickness, float startAngle, float endAngle,
                            const glm::u8vec4& color, std::optional<int> overrideDepth = std::nullopt,
                            const ClipRegion* clip = nullptr);

        void DrawCircleTextureVirtual(const glm::vec2& center, float radius, uint32_t virtualTextureID,
                                      const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                      std::optional<int> overrideDepth = std::nullopt,
                                      const ClipRegion* clip = nullptr);

        uint32_t DrawCircleTextureManaged(const glm::vec2& center, float radius,
                                          const nvrhi::TextureHandle& texture,
                                          const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                          std::optional<int> overrideDepth = std::nullopt,
                                          const ClipRegion* clip = nullptr);

        void DrawEllipseTextureVirtual(const glm::vec2& center, const glm::vec2& radii, float rotation,
                                       uint32_t virtualTextureID,
                                       const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                       std::optional<int> overrideDepth = std::nullopt,
                                       const ClipRegion* clip = nullptr);

        uint32_t DrawEllipseTextureManaged(const glm::vec2& center, const glm::vec2& radii, float rotation,
                                           const nvrhi::TextureHandle& texture,
                                           const glm::u8vec4& tintColor = glm::u8vec4(255, 255, 255, 255),
                                           std::optional<int> overrideDepth = std::nullopt,
                                           const ClipRegion* clip = nullptr);

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

        void RecalculateViewProjectionMatrix();

        nvrhi::DeviceHandle mDevice;
        glm::u32vec2 mOutputSize;
        glm::vec2 mVirtualSize;
        glm::mat4 mViewProjectionMatrix;

        nvrhi::TextureHandle mTexture;
        nvrhi::FramebufferHandle mFramebuffer;

        VirtualTextureManager mVirtualTextureManager;

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
}
