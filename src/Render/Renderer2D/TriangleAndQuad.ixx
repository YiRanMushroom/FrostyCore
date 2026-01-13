export module Render.Renderer2D:TriangleAndQuad;

import :ForwardDecleration;
import :Misc;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace
Engine {
    struct TriangleVertexData {
        glm::vec2 Position;
        glm::vec2 TexCoords;
        uint32_t InstanceIndex;
    };

    struct TriangleInstanceData {
        uint32_t TintColor;
        int32_t TextureIndex;
        int32_t ClipIndex; // < 0 means no clipping

        // now we add data for MTSDF font rendering
        InstanceRenderingMode RenderingMode = InstanceRenderingMode::Texture;
        float MTSDFPixelRange = 4.0f;

        uint32_t Padding[3];
    };

    static_assert(sizeof(TriangleInstanceData) % 16 == 0, "TriangleInstanceData size must be multiple of 16 bytes");

    export struct TriangleRenderingData {
        glm::mat4x2 Positions;
        glm::mat4x2 TexCoords;
        bool IsQuad;
        int VirtualTextureID;
        glm::u8vec4 TintColor;
        int Depth;
        int ClipRegionId;

        InstanceRenderingMode RenderingMode = InstanceRenderingMode::Texture;
        float MTSDFPixelRange = 4.0f;

        static TriangleRenderingData Triangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                              const glm::vec2 &p1, const glm::vec2 &uv1,
                                              const glm::vec2 &p2, const glm::vec2 &uv2,
                                              int textureIndex, glm::u8vec4 tintColor, int depth = 0,
                                              int clipRegionId = -1);

        static TriangleRenderingData Quad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                          const glm::vec2 &p1, const glm::vec2 &uv1,
                                          const glm::vec2 &p2, const glm::vec2 &uv2,
                                          const glm::vec2 &p3, const glm::vec2 &uv3,
                                          int virtualTextureID, glm::u8vec4 tintColor, int depth = 0,
                                          int clipRegionId = -1);

        static TriangleRenderingData QuadFont(const glm::vec2 &p0, const glm::vec2 &uv0,
                                              const glm::vec2 &p1, const glm::vec2 &uv1,
                                              const glm::vec2 &p2, const glm::vec2 &uv2,
                                              const glm::vec2 &p3, const glm::vec2 &uv3,
                                              int virtualTextureID, glm::u8vec4 tintColor,
                                              float MTSDFPixelRange, int depth = 0,
                                              int clipRegionId = -1);
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
                         int virtualTextureID, glm::u8vec4 tintColor, int depth,
                         int clipRegionId = -1);

        void AddQuad(const glm::vec2 &p0, const glm::vec2 &uv0,
                     const glm::vec2 &p1, const glm::vec2 &uv1,
                     const glm::vec2 &p2, const glm::vec2 &uv2,
                     const glm::vec2 &p3, const glm::vec2 &uv3,
                     int virtualTextureID, glm::u8vec4 tintColor, int depth,
                     int clipRegionId = -1);

        void AddQuadFont(const glm::vec2 &p0, const glm::vec2 &uv0,
                         const glm::vec2 &p1, const glm::vec2 &uv1,
                         const glm::vec2 &p2, const glm::vec2 &uv2,
                         const glm::vec2 &p3, const glm::vec2 &uv3,
                         int virtualTextureID, glm::u8vec4 tintColor,
                         float MTSDFPixelRange, int depth,
                         int clipRegionId = -1);

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
}