module Render.Renderer2D;

import :ForwardDecleration;
import :Misc;
import :Eclipse;

namespace
Engine {
    void EllipseRenderingSubmissionData::Clear() {
        ShapeData.clear();
    }

    void EllipseRenderingCommandList::Clear() {
        Instances.clear();
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
            shapeData.EntityID = instance.EntityID;

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