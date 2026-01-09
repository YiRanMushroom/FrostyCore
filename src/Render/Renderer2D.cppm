export module Render.Renderer2D;

import Vendor.ApplicationAPI;
import Core.Prelude;
import Render.GeneratedShaders;
import Render.VirtualTextureManager;
import glm;

namespace
Engine {
    export struct Renderer2DDescriptor {
        glm::u32vec2 OutputSize;
        glm::vec2 VirtualSize;
        nvrhi::DeviceHandle Device;
    };

    struct TriangleVertexData {
        glm::vec2 position;
        glm::vec2 texCoords;
        uint32_t constantIndex;
    };

    struct TriangleInstanceData {
        uint32_t tintColor;
        int32_t textureIndex;
    };

    struct TriangleRenderingData {
        struct VertexPosition {
            glm::vec2 position;
            glm::vec2 texCoords;
        };

        glm::mat4x2 Positions;
        glm::mat4x2 TexCoords;
        bool IsQuad;
        int VirtualTextureID;
        uint32_t TintColor;
        int Depth;

        static TriangleRenderingData Triangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                              const glm::vec2 &p1, const glm::vec2 &uv1,
                                              const glm::vec2 &p2, const glm::vec2 &uv2,
                                              int textureIndex, uint32_t tintColor, int depth = 0);

        static TriangleRenderingData Quad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                          const glm::vec2 &p1, const glm::vec2 &uv1,
                                          const glm::vec2 &p2, const glm::vec2 &uv2,
                                          const glm::vec2 &p3, const glm::vec2 &uv3,
                                          int virtualTextureID, uint32_t tintColor, int depth = 0);
    };

    struct TriangleRenderingSubmissionData {
        std::vector<TriangleVertexData> VertexData;
        std::vector<uint32_t> IndexData;
        std::vector<TriangleInstanceData> InstanceData;

        TriangleRenderingSubmissionData() = default;

        TriangleRenderingSubmissionData(TriangleRenderingSubmissionData &&) = default;

        TriangleRenderingSubmissionData &operator=(TriangleRenderingSubmissionData &&) = default;

        void Clear();
    };

    struct TriangleRenderingCommandList {
        std::vector<TriangleRenderingData> Instances;

        void Clear();

        void AddTriangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                         const glm::vec2 &p1, const glm::vec2 &uv1,
                         const glm::vec2 &p2, const glm::vec2 &uv2,
                         int virtualTextureID, uint32_t tintColor, int depth);

        void AddQuad(const glm::vec2 &p0, const glm::vec2 &uv0,
                     const glm::vec2 &p1, const glm::vec2 &uv1,
                     const glm::vec2 &p2, const glm::vec2 &uv2,
                     const glm::vec2 &p3, const glm::vec2 &uv3,
                     int virtualTextureID, uint32_t tintColor, int depth);

        std::vector<TriangleRenderingSubmissionData> RecordRendererSubmissionData(size_t triangleBufferInstanceSizeMax);

        void GiveBackForNextFrame(std::vector<TriangleRenderingSubmissionData> &&thisCache);

    private:
        std::vector<TriangleRenderingSubmissionData> mLastFrameCache;
    };

    struct TriangleBatchRenderingResources {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BufferHandle IndexBuffer;
        nvrhi::BufferHandle InstanceBuffer;
        nvrhi::BindingSetHandle mBindingSetSpace0;
    };

    struct LineVertexData {
        glm::vec2 position;
        uint32_t color;
    };

    struct LineRenderingSubmissionData {
        std::vector<LineVertexData> VertexData;

        void Clear();
    };

    struct LineRenderingCommandList {
        std::vector<LineVertexData> VertexData;

        void Clear();

        void AddLine(const glm::vec2 &p0, const glm::u8vec4 &color0,
                     const glm::vec2 &p1, const glm::u8vec4 &color1);

        std::vector<LineRenderingSubmissionData> RecordRendererSubmissionData(size_t lineBufferInstanceSizeMax);

        void GiveBackForNextFrame(std::vector<LineRenderingSubmissionData> &&thisCache);

    private:
        std::vector<LineRenderingSubmissionData> mLastFrameCache;
    };

    struct LineBatchRenderingResources {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BindingSetHandle mBindingSetSpace0;
    };

    struct EllipseShapeData {
        glm::vec2 center;
        glm::vec2 radii;
        float rotation;
        float innerScale;
        float startAngle;
        float endAngle;
        uint32_t tintColor;
        int32_t textureIndex;
        float edgeSoftness;
        float _padding;
    };

    struct EllipseRenderingData {
        glm::vec2 center;
        glm::vec2 radii;
        float rotation = 0.0f;
        float innerScale = 0.0f;
        float startAngle = 0.0f;
        float endAngle = 0.0f;
        int virtualTextureID = -1;
        uint32_t tintColor = 0xFFFFFFFF;
        float edgeSoftness = 1.0f;
        int depth = 0;

        static EllipseRenderingData Circle(const glm::vec2 &center, float radius,
                                           const glm::u8vec4 &color, int depth = 0);

        static EllipseRenderingData Ellipse(const glm::vec2 &center, const glm::vec2 &radii,
                                            float rotation, const glm::u8vec4 &color, int depth = 0);

        static EllipseRenderingData Ring(const glm::vec2 &center, float outerRadius, float innerRadius,
                                         const glm::u8vec4 &color, int depth = 0);

        static EllipseRenderingData Sector(const glm::vec2 &center, float radius,
                                           float startAngle, float endAngle,
                                           const glm::u8vec4 &color, int textureIndex = -1, int depth = 0);

        static EllipseRenderingData Arc(const glm::vec2 &center, float radius, float thickness,
                                        float startAngle, float endAngle,
                                        const glm::u8vec4 &color, int depth = 0);

        static EllipseRenderingData EllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                                  float rotation, float startAngle, float endAngle,
                                                  const glm::u8vec4 &color, int textureIndex = -1, int depth = 0);

        static EllipseRenderingData EllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                               float rotation, float thickness,
                                               float startAngle, float endAngle,
                                               const glm::u8vec4 &color, int depth = 0);
    };

    struct EllipseRenderingSubmissionData {
        std::vector<EllipseShapeData> ShapeData;

        EllipseRenderingSubmissionData() = default;

        EllipseRenderingSubmissionData(EllipseRenderingSubmissionData &&) = default;

        EllipseRenderingSubmissionData &operator=(EllipseRenderingSubmissionData &&) = default;

        void Clear();
    };

    struct EllipseRenderingCommandList {
        std::vector<EllipseRenderingData> Instances;

        void Clear();

        void AddEllipse(const EllipseRenderingData &data);

        std::vector<EllipseRenderingSubmissionData> RecordRendererSubmissionData(size_t ellipseBufferInstanceSizeMax);

        void GiveBackForNextFrame(std::vector<EllipseRenderingSubmissionData> &&thisCache);

    private:
        std::vector<EllipseRenderingSubmissionData> mLastFrameCache;
    };

    struct EllipseBatchRenderingResources {
        nvrhi::BufferHandle ShapeBuffer;
        nvrhi::BindingSetHandle mBindingSetSpace0;
    };

    export class Renderer2D {
    public:
        Renderer2D(Renderer2DDescriptor desc);

        void BeginRendering();

        void EndRendering();

        void OnResize(uint32_t width, uint32_t height);

        [[nodiscard]] nvrhi::ITexture *GetTexture() const;

        void Clear();

        [[nodiscard]] int GetCurrentDepth() const;

        void SetCurrentDepth(int depth);

        uint32_t RegisterVirtualTextureForThisFrame(const nvrhi::TextureHandle &texture);

        void DrawTriangleColored(const glm::mat3x2 &positions, const glm::u8vec4 &color,
                                 std::optional<int> overrideDepth = std::nullopt);

        void DrawTriangleTextureVirtual(const glm::mat3x2 &positions, const glm::mat3x2 &uvs,
                                        uint32_t virtualTextureID, std::optional<int> overrideDepth = std::nullopt,
                                        glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255));

        uint32_t DrawTriangleTextureManaged(const glm::mat3x2 &positions, const glm::mat3x2 &uvs,
                                            const nvrhi::TextureHandle &texture,
                                            std::optional<int> overrideDepth = std::nullopt,
                                            glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255));

        void DrawQuadColored(const glm::mat4x2 &positions, const glm::u8vec4 &color,
                             std::optional<int> overrideDepth = std::nullopt);

        void DrawQuadTextureVirtual(const glm::mat4x2 &positions, const glm::mat4x2 &uvs,
                                    uint32_t virtualTextureID, std::optional<int> overrideDepth = std::nullopt,
                                    glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255));

        uint32_t DrawQuadTextureManaged(const glm::mat4x2 &positions, const glm::mat4x2 &uvs,
                                        const nvrhi::TextureHandle &texture,
                                        std::optional<int> overrideDepth = std::nullopt,
                                        glm::u8vec4 tintColor = glm::u8vec4(255, 255, 255, 255));

        void DrawLine(const glm::vec2 &p0, const glm::vec2 &p1, const glm::u8vec4 &color);

        void DrawLine(const glm::vec2 &p0, const glm::vec2 &p1,
                      const glm::u8vec4 &color0, const glm::u8vec4 &color1);

        void DrawCircle(const glm::vec2 &center, float radius, const glm::u8vec4 &color,
                        std::optional<int> overrideDepth = std::nullopt);

        void DrawEllipse(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                         const glm::u8vec4 &color, std::optional<int> overrideDepth = std::nullopt);

        void DrawRing(const glm::vec2 &center, float outerRadius, float innerRadius,
                      const glm::u8vec4 &color, std::optional<int> overrideDepth = std::nullopt);

        void DrawSector(const glm::vec2 &center, float radius, float startAngle, float endAngle,
                        const glm::u8vec4 &color, std::optional<int> overrideDepth = std::nullopt);

        void DrawSectorTextureVirtual(const glm::vec2 &center, float radius, float startAngle, float endAngle,
                                      uint32_t virtualTextureID,
                                      const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                      std::optional<int> overrideDepth = std::nullopt);

        uint32_t DrawSectorTextureManaged(const glm::vec2 &center, float radius, float startAngle, float endAngle,
                                          const nvrhi::TextureHandle &texture,
                                          const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                          std::optional<int> overrideDepth = std::nullopt);

        void DrawArc(const glm::vec2 &center, float radius, float thickness,
                     float startAngle, float endAngle, const glm::u8vec4 &color,
                     std::optional<int> overrideDepth = std::nullopt);

        void DrawEllipseSector(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                               float startAngle, float endAngle, const glm::u8vec4 &color,
                               std::optional<int> overrideDepth = std::nullopt);

        void DrawEllipseSectorTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                                             float startAngle, float endAngle, uint32_t virtualTextureID,
                                             const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                             std::optional<int> overrideDepth = std::nullopt);

        void DrawEllipseArc(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                            float thickness, float startAngle, float endAngle,
                            const glm::u8vec4 &color, std::optional<int> overrideDepth = std::nullopt);

        void DrawCircleTextureVirtual(const glm::vec2 &center, float radius, uint32_t virtualTextureID,
                                      const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                      std::optional<int> overrideDepth = std::nullopt);

        uint32_t DrawCircleTextureManaged(const glm::vec2 &center, float radius,
                                          const nvrhi::TextureHandle &texture,
                                          const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                          std::optional<int> overrideDepth = std::nullopt);

        void DrawEllipseTextureVirtual(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                                       uint32_t virtualTextureID,
                                       const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                       std::optional<int> overrideDepth = std::nullopt);

        uint32_t DrawEllipseTextureManaged(const glm::vec2 &center, const glm::vec2 &radii, float rotation,
                                           const nvrhi::TextureHandle &texture,
                                           const glm::u8vec4 &tintColor = glm::u8vec4(255, 255, 255, 255),
                                           std::optional<int> overrideDepth = std::nullopt);

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
