export module Render.Renderer2D:DrawCommands;

import :ForwardDecleration;
import :Misc;
import :TriangleAndQuad;
import :Line;
import :Eclipse;
import Core.Prelude;

namespace
Engine {
    export struct DrawCommand {};

    export class TriangleDrawCommand : public virtual DrawCommand {
    public:
        TriangleDrawCommand() = default;

        TriangleDrawCommand &SetPositions(const glm::vec2 &p0,
                                          const glm::vec2 &p1,
                                          const glm::vec2 &p2) {
            Positions = {p0, p1, p2};
            return *this;
        }

        TriangleDrawCommand &SetFirstPoint(const glm::vec2 &p0) {
            Positions[0] = p0;
            return *this;
        }

        TriangleDrawCommand &SetSecondPoint(const glm::vec2 &p1) {
            Positions[1] = p1;
            return *this;
        }

        TriangleDrawCommand &SetThirdPoint(const glm::vec2 &p2) {
            Positions[2] = p2;
            return *this;
        }

        TriangleDrawCommand &SetUVs(const glm::vec2 &uv0,
                                    const glm::vec2 &uv1,
                                    const glm::vec2 &uv2) {
            UVs = {uv0, uv1, uv2};
            return *this;
        }

        TriangleDrawCommand &SetFirstUV(const glm::vec2 &uv0) {
            UVs[0] = uv0;
            return *this;
        }

        TriangleDrawCommand &SetSecondUV(const glm::vec2 &uv1) {
            UVs[1] = uv1;
            return *this;
        }

        TriangleDrawCommand &SetThirdUV(const glm::vec2 &uv2) {
            UVs[2] = uv2;
            return *this;
        }

        TriangleDrawCommand &SetTexture(int virtualTextureID) {
            VirtualTextureID = virtualTextureID;
            return *this;
        }

        TriangleDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            TintColor = tintColor;
            return *this;
        }

        TriangleDrawCommand &SetDepth(int overrideDepth) {
            OverrideDepth = overrideDepth;
            return *this;
        }

        TriangleDrawCommand &SetClipRegionId(int clipRegionId) {
            ClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    public:
        std::array<glm::vec2, 3> Positions;
        std::array<glm::vec2, 3> UVs;
        int VirtualTextureID;
        glm::u8vec4 TintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        std::optional<int> OverrideDepth = std::nullopt;
        int ClipRegionId = -1;
    };

    export class QuadDrawCommand : public virtual DrawCommand {
    public:
        QuadDrawCommand() = default;

        QuadDrawCommand &SetPositions(const glm::vec2 &p0,
                                      const glm::vec2 &p2) {
            FirstPoint = p0;
            SecondPoint = p2;
            return *this;
        }

        QuadDrawCommand &SetFirstPoint(const glm::vec2 &p0) {
            FirstPoint = p0;
            return *this;
        }

        QuadDrawCommand &SetSecondPoint(const glm::vec2 &p2) {
            SecondPoint = p2;
            return *this;
        }

        QuadDrawCommand &SetUVs(const glm::vec2 &uv0,
                                const glm::vec2 &uv1) {
            FirstUV = uv0;
            SecondUV = uv1;
            return *this;
        }

        QuadDrawCommand &SetFirstUV(const glm::vec2 &uv0) {
            FirstUV = uv0;
            return *this;
        }

        QuadDrawCommand &SetSecondUV(const glm::vec2 &uv1) {
            SecondUV = uv1;
            return *this;
        }

        QuadDrawCommand &SetTexture(int virtualTextureID) {
            VirtualTextureID = virtualTextureID;
            RenderingMode = InstanceRenderingMode::Texture;
            return *this;
        }

        QuadDrawCommand &SetFontAtlas(int virtualTextureID, float MTSDFPixelRange) {
            VirtualTextureID = virtualTextureID;
            RenderingMode = InstanceRenderingMode::MTSDF;
            this->MTSDFPixelRange = MTSDFPixelRange;
            return *this;
        }

        QuadDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            TintColor = tintColor;
            return *this;
        }

        QuadDrawCommand &SetDepth(int overrideDepth) {
            OverrideDepth = overrideDepth;
            return *this;
        }

        QuadDrawCommand &SetClipRegionId(int clipRegionId) {
            ClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    public:
        glm::vec2 FirstPoint;
        glm::vec2 SecondPoint;

        glm::vec2 FirstUV = glm::vec2(0.0f, 0.0f);
        glm::vec2 SecondUV = glm::vec2(1.0f, 1.0f);

        int VirtualTextureID;
        glm::u8vec4 TintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        std::optional<int> OverrideDepth = std::nullopt;
        int ClipRegionId = -1;

        InstanceRenderingMode RenderingMode = InstanceRenderingMode::Texture;
        float MTSDFPixelRange;
    };

    // No need to provide draw command for line because it is simple enough

    // Now we add circular draw command.
    export class CircularDrawCommand : public virtual DrawCommand {
    public:
        CircularDrawCommand() = default;

        CircularDrawCommand &SetCenter(const glm::vec2 &center) {
            Center = center;
            return *this;
        }

        CircularDrawCommand &SetRadii(const glm::vec2 &radii) {
            Radii = radii;
            return *this;
        }

        CircularDrawCommand &SetRadius(float radius) {
            Radii = glm::vec2(radius, radius);
            return *this;
        }

        CircularDrawCommand &SetRotation(float rotation) {
            Rotation = rotation;
            return *this;
        }

        CircularDrawCommand &SetStartAngle(float startAngle) {
            StartAngle = startAngle;
            return *this;
        }

        CircularDrawCommand &SetEndAngle(float endAngle) {
            EndAngle = endAngle;
            return *this;
        }

        CircularDrawCommand &SetTexture(int virtualTextureID) {
            VirtualTextureID = virtualTextureID;
            return *this;
        }

        CircularDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            TintColor = tintColor;
            return *this;
        }

        CircularDrawCommand &SetEdgeSoftness(float edgeSoftness) {
            EdgeSoftness = edgeSoftness;
            return *this;
        }

        CircularDrawCommand &SetDepth(int overrideDepth) {
            OverrideDepth = overrideDepth;
            return *this;
        }

        CircularDrawCommand &SetClipRegionId(int clipRegionId) {
            ClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    public:
        glm::vec2 Center;
        glm::vec2 Radii;
        float Rotation = 0.0f;
        float InnerScale = 0.0f;
        float StartAngle = 0.0f;
        float EndAngle = std::numbers::pi_v<float> * 2.0f;
        int VirtualTextureID = -1;
        glm::u8vec4 TintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        float EdgeSoftness = 1.0f;
        std::optional<int> OverrideDepth = 0;
        int ClipRegionId = -1;
    };
}
