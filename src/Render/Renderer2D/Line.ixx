export module Render.Renderer2D:Line;

import :ForwardDecleration;
import :Misc;
import Core.Prelude;
import Vendor.ApplicationAPI;

namespace Engine {
    struct LineVertexData {
        glm::vec2 Position;
        uint32_t Color;
    };

    struct LineRenderingData {
        glm::mat2x2 Positions;
        glm::u8vec4 Color;
    };

    struct LineRenderingSubmissionData {
        std::vector<LineVertexData> VertexData;

        void Clear();
    };

    struct LineRenderingCommandList {
        std::vector<LineRenderingData> RenderingData;

        void Clear();

        void AddLine(const glm::vec2& p0,
                     const glm::vec2& p1, glm::u8vec4 color);

        std::vector<LineRenderingSubmissionData> RecordRendererSubmissionData(size_t lineBufferInstanceSizeMax);

        void GiveBackForNextFrame(std::vector<LineRenderingSubmissionData>&& thisCache);

    private:
        std::vector<LineRenderingSubmissionData> mLastFrameCache;
    };

    struct LineBatchRenderingResources {
        nvrhi::BufferHandle VertexBuffer;
        nvrhi::BindingSetHandle mBindingSetSpace0;
    };
}

