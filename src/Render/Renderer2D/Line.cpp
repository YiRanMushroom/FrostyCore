module Render.Renderer2D;

import :ForwardDecleration;
import :Misc;
import :Line;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace Engine {
    void LineRenderingSubmissionData::Clear() {
        VertexData.clear();
    }

    void LineRenderingCommandList::Clear() {
        RenderingData.clear();
    }

    void LineRenderingCommandList::AddLine(const glm::vec2 &p0,
                                           const glm::vec2 &p1, glm::u8vec4 color) {
        RenderingData.push_back({
            .Positions = {p0, p1},
            .Color = color
        });
    }


    std::vector<LineRenderingSubmissionData> LineRenderingCommandList::RecordRendererSubmissionData(
        size_t lineBufferInstanceSizeMax) {
        std::vector<LineRenderingSubmissionData> submissions;
        if (RenderingData.empty()) return submissions;

        auto lastFrameSubmissionIt = mLastFrameCache.begin();

        LineRenderingSubmissionData currentSubmission;
        if (lastFrameSubmissionIt != mLastFrameCache.end()) {
            currentSubmission = std::move(*lastFrameSubmissionIt);
            currentSubmission.VertexData.clear();
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
                    ++lastFrameSubmissionIt;
                }
            }
        };

        for (const auto &renderingData: RenderingData) {
            // check if we need to finalize due to vertex buffer size
            if (currentSubmission.VertexData.size() + 1 >
                lineBufferInstanceSizeMax) {
                finalizeSubmission();
            }

            currentSubmission.VertexData.reserve(currentSubmission.VertexData.size() + 2);
            currentSubmission.VertexData.push_back({
                .Position = renderingData.Positions[0],
                .Color = static_cast<uint32_t>(renderingData.Color.r) << 24 | renderingData.Color.g << 16 |
                         renderingData.Color.b << 8 | renderingData.Color.a
            });
            currentSubmission.VertexData.push_back({
                .Position = renderingData.Positions[1],
                .Color = static_cast<uint32_t>(renderingData.Color.r) << 24 | renderingData.Color.g << 16 |
                         renderingData.Color.b << 8 | renderingData.Color.a
            });
        }

        finalizeSubmission();

        return submissions;
    }

    void LineRenderingCommandList::GiveBackForNextFrame(std::vector<LineRenderingSubmissionData> &&thisCache) {
        mLastFrameCache = std::move(thisCache);
        mLastFrameCache.resize(0);
    }
}