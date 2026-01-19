module Render.Renderer2D;

import :ForwardDecleration;
import :Misc;
import :Eclipse;

namespace
Engine {
    EllipseRenderingData EllipseRenderingData::Circle(const glm::vec2 &center, float radius,
                                                      const glm::u8vec4 &color, int depth,
                                                      int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Ellipse(const glm::vec2 &center, const glm::vec2 &radii,
                                                       float rotation, const glm::u8vec4 &color, int depth,
                                                       int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Ring(const glm::vec2 &center, float outerRadius, float innerRadius,
                                                    const glm::u8vec4 &color, int depth,
                                                    int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(outerRadius, outerRadius);
        data.InnerScale = innerRadius / outerRadius;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Sector(const glm::vec2 &center, float radius,
                                                      float startAngle, float endAngle,
                                                      const glm::u8vec4 &color, int textureIndex, int depth,
                                                      int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.VirtualTextureID = textureIndex;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::Arc(const glm::vec2 &center, float radius, float thickness,
                                                   float startAngle, float endAngle,
                                                   const glm::u8vec4 &color, int depth,
                                                   int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = glm::vec2(radius, radius);
        data.InnerScale = (radius - thickness) / radius;
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::EllipseSector(const glm::vec2 &center, const glm::vec2 &radii,
                                                             float rotation, float startAngle, float endAngle,
                                                             const glm::u8vec4 &color, int textureIndex, int depth,
                                                             int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.VirtualTextureID = textureIndex;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    EllipseRenderingData EllipseRenderingData::EllipseArc(const glm::vec2 &center, const glm::vec2 &radii,
                                                          float rotation, float thickness,
                                                          float startAngle, float endAngle,
                                                          const glm::u8vec4 &color, int depth,
                                                          int clipRegionId) {
        EllipseRenderingData data;
        data.Center = center;
        data.Radii = radii;
        data.Rotation = rotation;
        float minRadius = glm::min(radii.x, radii.y);
        data.InnerScale = glm::max(0.0f, (minRadius - thickness) / minRadius);
        data.StartAngle = startAngle;
        data.EndAngle = endAngle;
        data.TintColor = color;
        data.Depth = depth;
        data.ClipRegionId = clipRegionId;
        return data;
    }

    void EllipseRenderingSubmissionData::Clear() {
        ShapeData.clear();
    }

    void EllipseRenderingCommandList::Clear() {
        Instances.clear();
    }

    void EllipseRenderingCommandList::AddEllipse(const EllipseRenderingData &data) {
        Instances.push_back(data);
    }

    std::vector<EllipseRenderingSubmissionData> EllipseRenderingCommandList::RecordRendererSubmissionData(
        size_t ellipseBufferInstanceSizeMax) {
        std::ranges::sort(Instances, [](const EllipseRenderingData &a, const EllipseRenderingData &b)-> bool {
            if (a.Depth != b.Depth) return a.Depth < b.Depth;
            return a.VirtualTextureID < b.VirtualTextureID;
        });


        std::vector<EllipseRenderingSubmissionData> submissions;
        if (Instances.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        EllipseRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.ShapeData.clear();
            ++lastFrameSubmissionIt;
        }

        auto finalizeSubmission = [&]() mutable {
            if (!currentSubmission.ShapeData.empty()) {
                submissions.push_back(std::move(currentSubmission));

                if (lastFrameSubmissionIt == mLastFrameCache.end()) {
                    currentSubmission.Clear();
                } else {
                    currentSubmission = std::move(*lastFrameSubmissionIt);
                    currentSubmission.ShapeData.clear();
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &instance: Instances) {
            if (currentSubmission.ShapeData.size() + 1 > ellipseBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            // ClipRegionId is already set correctly in the instance
            int32_t clipIndex = instance.ClipRegionId;

            EllipseShapeData shapeData;
            shapeData.Center = instance.Center;
            shapeData.Radii = instance.Radii;
            shapeData.Rotation = instance.Rotation;
            shapeData.InnerScale = instance.InnerScale;
            shapeData.StartAngle = instance.StartAngle;
            shapeData.EndAngle = instance.EndAngle;
            shapeData.TintColor = static_cast<uint32_t>(instance.TintColor.r) << 24 | instance.TintColor.g << 16 |
                                  instance.TintColor.b << 8 | instance.TintColor.a;
            shapeData.TextureIndex = instance.VirtualTextureID;
            shapeData.EdgeSoftness = instance.EdgeSoftness;
            shapeData.ClipRegionId = clipIndex;
            shapeData.ModelMatrix = instance.ModelMatrix;

            currentSubmission.ShapeData.push_back(shapeData);
        }

        finalizeSubmission();

        return submissions;
    }

    void EllipseRenderingCommandList::GiveBackForNextFrame(std::vector<EllipseRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }
}