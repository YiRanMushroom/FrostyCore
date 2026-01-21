module Render.Renderer2D; // part TriangleAndQuad;

import :ForwardDecleration;
import :Misc;
import Core.Prelude;

namespace
Engine {
    ClipRegion ClipRegion::Triangle(const glm::mat3x2 &points, Frosty::ClipMode clipMode) {
        return ClipRegion{
            .Points = {
                points[0],
                points[1],
                points[2],
                {}
            },
            .PointCount = 3,
            .ClipMode = clipMode
        };
    }

    ClipRegion ClipRegion::Quad(const glm::mat2x2 &points, Frosty::ClipMode clipMode) {
        return ClipRegion{
            .Points = {
                points[0],
                {points[1].x, points[0].y},
                points[1],
                {points[0].x, points[1].y}
            },
            .PointCount = 4,
            .ClipMode = clipMode
        };
    }

    TriangleRenderingData TriangleRenderingData::Triangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                          const glm::vec2 &p1, const glm::vec2 &uv1,
                                                          const glm::vec2 &p2, const glm::vec2 &uv2,
                                                          int textureIndex,
                                                          glm::u8vec4 tintColor, int depth,
                                                          int clipRegionId) {
        return {
            .Positions = {p0, p1, p2, {}},
            .TexCoords = {uv0, uv1, uv2, {}},
            .IsQuad = false,
            .VirtualTextureID = textureIndex,
            .TintColor = tintColor,
            .Depth = depth,
            .ClipRegionId = clipRegionId
        };
    }

    TriangleRenderingData TriangleRenderingData::Quad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                      const glm::vec2 &p1, const glm::vec2 &uv1,
                                                      const glm::vec2 &p2, const glm::vec2 &uv2,
                                                      const glm::vec2 &p3, const glm::vec2 &uv3,
                                                      int virtualTextureID,
                                                      glm::u8vec4 tintColor, int depth,
                                                      int clipRegionId) {
        return {
            .Positions = {p0, p1, p2, p3},
            .TexCoords = {uv0, uv1, uv2, uv3},
            .IsQuad = true,
            .VirtualTextureID = virtualTextureID,
            .TintColor = tintColor,
            .Depth = depth,
            .ClipRegionId = clipRegionId
        };
    }

    TriangleRenderingData TriangleRenderingData::QuadFont(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                          const glm::vec2 &p1,
                                                          const glm::vec2 &uv1, const glm::vec2 &p2,
                                                          const glm::vec2 &uv2, const glm::vec2 &p3,
                                                          const glm::vec2 &uv3,
                                                          int virtualTextureID, glm::u8vec4 tintColor,
                                                          float MTSDFPixelRange, int depth, int clipRegionId) {
        return {
            .Positions = {p0, p1, p2, p3},
            .TexCoords = {uv0, uv1, uv2, uv3},
            .IsQuad = true,
            .VirtualTextureID = virtualTextureID,
            .TintColor = tintColor,
            .Depth = depth,
            .ClipRegionId = clipRegionId,
            .RenderingMode = InstanceRenderingMode::MTSDF,
            .MTSDFPixelRange = MTSDFPixelRange
        };
    }

    void TriangleRenderingSubmissionData::Clear() {
        VertexData.clear();
        IndexData.clear();
        InstanceData.clear();
    }

    void TriangleRenderingCommandList::AddTriangle(const glm::vec2 &p0, const glm::vec2 &uv0,
                                                   const glm::vec2 &p1, const glm::vec2 &uv1,
                                                   const glm::vec2 &p2, const glm::vec2 &uv2,
                                                   int virtualTextureID,
                                                   glm::u8vec4 tintColor,
                                                   int depth, int clipRegionId) {
        Instances.emplace_back(
            TriangleRenderingData::Triangle(
                p0, uv0, p1, uv1, p2, uv2, virtualTextureID, tintColor, depth, clipRegionId));
    }

    void TriangleRenderingCommandList::AddQuad(const glm::vec2 &p0, const glm::vec2 &uv0,
                                               const glm::vec2 &p1, const glm::vec2 &uv1,
                                               const glm::vec2 &p2, const glm::vec2 &uv2,
                                               const glm::vec2 &p3, const glm::vec2 &uv3,
                                               int virtualTextureID,
                                               glm::u8vec4 tintColor,
                                               int depth, int clipRegionId) {
        Instances.emplace_back(
            TriangleRenderingData::Quad(
                p0, uv0, p1, uv1, p2, uv2, p3, uv3, virtualTextureID, tintColor, depth, clipRegionId));
    }

    void TriangleRenderingCommandList::AddQuadFont(const glm::vec2 &p0, const glm::vec2 &uv0, const glm::vec2 &p1,
                                                   const glm::vec2 &uv1, const glm::vec2 &p2, const glm::vec2 &uv2,
                                                   const glm::vec2 &p3, const glm::vec2 &uv3,
                                                   int virtualTextureID, glm::u8vec4 tintColor, float MTSDFPixelRange,
                                                   int depth, int clipRegionId) {
        Instances.emplace_back(
            TriangleRenderingData::QuadFont(
                p0, uv0, p1, uv1, p2, uv2, p3, uv3, virtualTextureID, tintColor, MTSDFPixelRange, depth, clipRegionId));
    }

    void TriangleRenderingCommandList::Clear() {
        Instances.clear();
    }

    std::vector<TriangleRenderingSubmissionData> TriangleRenderingCommandList::RecordRendererSubmissionData(
        size_t triangleBufferInstanceSizeMax) {
        std::ranges::sort(Instances, [](const auto &a, const auto &b) {
            if (a.Depth != b.Depth) return a.Depth < b.Depth;
            if (a.RenderingMode != b.RenderingMode) return a.RenderingMode < b.RenderingMode;
            return a.VirtualTextureID < b.VirtualTextureID;
        });

        std::vector<TriangleRenderingSubmissionData> submissions;
        if (Instances.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        TriangleRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.VertexData.clear();
            currentSubmission.IndexData.clear();
            currentSubmission.InstanceData.clear();
            ++lastFrameSubmissionIt;
        }

        auto finalizeSubmission = [&]() mutable {
            if (!currentSubmission.VertexData.empty()) {
                submissions.push_back(std::move(currentSubmission));

                if (lastFrameSubmissionIt == mLastFrameCache.end()) {
                    currentSubmission.Clear();
                } else {
                    currentSubmission = std::move(*lastFrameSubmissionIt);
                    currentSubmission.VertexData.clear();
                    currentSubmission.IndexData.clear();
                    currentSubmission.InstanceData.clear();
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &instance: Instances) {
            // check if we need to finalize due to vertex/index buffer size
            if (currentSubmission.InstanceData.size() + 1 >
                triangleBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            int32_t finalTextureIndex = instance.VirtualTextureID;

            // ClipRegionId is already set correctly in the instance
            int32_t clipIndex = instance.ClipRegionId;

            // Fill Instance Data
            auto instanceIndex = static_cast<uint32_t>(currentSubmission.InstanceData.size());

            currentSubmission.InstanceData.push_back({
                .TintColor = static_cast<uint32_t>(instance.TintColor.r) << 24 | instance.TintColor.g << 16 | instance.
                             TintColor.b << 8 | instance.TintColor.a,
                .TextureIndex = finalTextureIndex,
                .ClipIndex = clipIndex,
                .RenderingMode = instance.RenderingMode,
                .MTSDFPixelRange = instance.MTSDFPixelRange,
                .EntityID = instance.EntityID,
                .ModelMatrix = instance.ModelMatrix
            });

            uint32_t baseVtx = static_cast<uint32_t>(currentSubmission.VertexData.size());

            if (!instance.IsQuad) {
                currentSubmission.VertexData.resize(currentSubmission.VertexData.size() + 3);
                for (int i = 0; i < 3; ++i) {
                    TriangleVertexData *v = &currentSubmission.VertexData[baseVtx + i];
                    v->Position = instance.Positions[i];
                    v->TexCoords = instance.TexCoords[i];
                    v->InstanceIndex = instanceIndex;
                }
                currentSubmission.IndexData.resize(currentSubmission.IndexData.size() + 3);
                uint32_t *idx0 = &currentSubmission.IndexData[currentSubmission.IndexData.size() - 3];
                idx0[0] = baseVtx + 0;
                idx0[1] = baseVtx + 1;
                idx0[2] = baseVtx + 2;
            } else {
                // Quad (Assume TL, TR, BR, BL)
                currentSubmission.VertexData.resize(currentSubmission.VertexData.size() + 4);
                for (int i = 0; i < 4; ++i) {
                    TriangleVertexData *v = &currentSubmission.VertexData[baseVtx + i];
                    v->Position = instance.Positions[i];
                    v->TexCoords = instance.TexCoords[i];
                    v->InstanceIndex = instanceIndex;
                }
                currentSubmission.IndexData.resize(currentSubmission.IndexData.size() + 6);
                uint32_t *idx0 = &currentSubmission.IndexData[currentSubmission.IndexData.size() - 6];
                idx0[0] = baseVtx + 0;
                idx0[1] = baseVtx + 1;
                idx0[2] = baseVtx + 2;
                idx0[3] = baseVtx + 0;
                idx0[4] = baseVtx + 2;
                idx0[5] = baseVtx + 3;
            }
        }

        finalizeSubmission();

        return submissions;
    }

    void TriangleRenderingCommandList::GiveBackForNextFrame(std::vector<TriangleRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }
}
