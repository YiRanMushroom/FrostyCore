export module Render.Renderer2D:Misc;

import :ForwardDecleration;
import Core.Prelude;

namespace
Engine {
    export enum class ClipMode : uint32_t {
        ShowInside = 0, // Clip outside
        ShowOutside = 1 // Clip inside
    };

    export struct ClipRegion {
        glm::mat4x2 Points; // Virtual coordinates
        uint32_t PointCount; // 3 or 4
        ClipMode ClipMode; // 0 = show inside (clip outside), 1 = show outside (clip inside)

        static ClipRegion Triangle(const glm::mat3x2 &points,
                                   Engine::ClipMode clipMode = Engine::ClipMode::ShowInside);

        static ClipRegion Quad(const glm::mat4x2 &points,
                               Engine::ClipMode clipMode = Engine::ClipMode::ShowInside);
    };

    enum class InstanceRenderingMode: uint32_t {
        Texture = 0,
        MTSDF = 1
    };
}
