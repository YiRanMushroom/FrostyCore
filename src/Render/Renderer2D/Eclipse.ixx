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

        static EllipseRenderingData Circle(const glm::vec2 &center, float radius,
                                           const glm::u8vec4 &color, int depth = 0,
                                           int clipRegionId = -1);

        static EllipseRenderingData Ellipse(const glm::vec2 &center, const glm::vec2 &radii,
                                            float rotation, const glm::u8vec4 &color, int depth = 0,
                                            int clipRegionId = -1);

        static EllipseRenderingData Ring(const glm::vec2 &center, float outerRadius, float innerRadius,
                                         const glm::u8vec4 &color, int depth = 0,
                                         int clipRegionId = -1);

        static EllipseRenderingData Sector(const glm::vec2 &center, float radius,
                                           float startAngle, float endAngle,
                                           const glm::u8vec4 &color, int textureIndex = -1, int depth = 0,
                                           int clipRegionId = -1);

        static EllipseRenderingData Arc(const glm::vec2 &center, float radius, float thickness,
                                        float startAngle, float endAngle,
                                        const glm::u8vec4 &color, int depth = 0,
                                        int clipRegionId = -1);

        static EllipseRenderingData EllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                                  float rotation, float startAngle, float endAngle,
                                                  const glm::u8vec4 &color, int textureIndex = -1, int depth = 0,
                                                  int clipRegionId = -1);

        static EllipseRenderingData EllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                               float rotation, float thickness,
                                               float startAngle, float endAngle,
                                               const glm::u8vec4 &color, int depth = 0,
                                               int clipRegionId = -1);
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
}
