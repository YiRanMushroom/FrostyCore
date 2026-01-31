export module Render.Renderer2D:Eclipse;

import :ForwardDecleration;
import :Misc;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace
Engine {
    struct EllipseShapeData {
        glm::vec2 Center;
        glm::vec2 Radii;
        float Rotation;
        float InnerScale;
        float StartAngle;
        float EndAngle;
        uint32_t TintColor;
        int32_t TextureIndex;
        float EdgeSoftness;
        int32_t ClipRegionId; // -1 means no clipping
        uint32_t EntityID;

        uint32_t Padding[3]; // Align ModelMatrix to 16-byte boundary (HLSL requirement)

        glm::mat4x4 ModelMatrix = glm::mat4x4(1.0f); // just transform vertices in vs should be fine
    };

    export struct EllipseRenderingData {
        glm::vec2 Center;
        glm::vec2 Radii;
        float Rotation = 0.0f;
        float InnerScale = 0.0f;
        float StartAngle = 0.0f;
        float EndAngle = 0.0f;
        int VirtualTextureID = -1;
        glm::u8vec4 TintColor = glm::u8vec4(255u, 255u, 255u, 255u);
        float EdgeSoftness = 1.0f;
        int Depth = 0;
        int ClipRegionId = -1;
        uint32_t EntityID = 0;
        glm::mat4x4 ModelMatrix = glm::mat4x4(1.0f);
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

        std::vector<EllipseRenderingSubmissionData> RecordRendererSubmissionData(size_t ellipseBufferInstanceSizeMax);

        void GiveBackForNextFrame(std::vector<EllipseRenderingSubmissionData> &&thisCache);

    private:
        std::vector<EllipseRenderingSubmissionData> mLastFrameCache;
    };

    struct EllipseBatchRenderingResources {
        nvrhi::BufferHandle ShapeBuffer;
        nvrhi::BindingSetHandle mBindingSetSpace0;
    };
}
