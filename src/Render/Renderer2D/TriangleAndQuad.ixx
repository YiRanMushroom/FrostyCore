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

        uint32_t EntityID = 0;
        uint32_t Padding[2]; // Align ModelMatrix to 16-byte boundary (HLSL requirement)

        glm::mat4x4 ModelMatrix = glm::mat4x4(1.0f);
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
        uint32_t EntityID = 0;

        InstanceRenderingMode RenderingMode = InstanceRenderingMode::Texture;
        float MTSDFPixelRange = 4.0f;

        glm::mat4x4 ModelMatrix = glm::mat4x4(1.0f);
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