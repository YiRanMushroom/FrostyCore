export module Render.Renderer2D:DrawCommands;

import :ForwardDecleration;
import :Misc;
import :TriangleAndQuad;
import :Line;
import :Eclipse;
import Core.Prelude;

namespace
Engine {
    struct DrawCommand {};

    export class TriangleDrawCommand : public virtual DrawCommand {
    public:
        TriangleDrawCommand() = default;

        TriangleDrawCommand &SetPositions(const glm::vec2 &p0,
                                          const glm::vec2 &p1,
                                          const glm::vec2 &p2) {
            mPositions = {p0, p1, p2};
            return *this;
        }

        TriangleDrawCommand &SetFirstPoint(const glm::vec2 &p0) {
            mPositions[0] = p0;
            return *this;
        }

        TriangleDrawCommand &SetSecondPoint(const glm::vec2 &p1) {
            mPositions[1] = p1;
            return *this;
        }

        TriangleDrawCommand &SetThirdPoint(const glm::vec2 &p2) {
            mPositions[2] = p2;
            return *this;
        }

        TriangleDrawCommand &SetUVs(const glm::vec2 &uv0,
                                    const glm::vec2 &uv1,
                                    const glm::vec2 &uv2) {
            mUVs = {uv0, uv1, uv2};
            return *this;
        }

        TriangleDrawCommand &SetFirstUV(const glm::vec2 &uv0) {
            mUVs[0] = uv0;
            return *this;
        }

        TriangleDrawCommand &SetSecondUV(const glm::vec2 &uv1) {
            mUVs[1] = uv1;
            return *this;
        }

        TriangleDrawCommand &SetThirdUV(const glm::vec2 &uv2) {
            mUVs[2] = uv2;
            return *this;
        }

        TriangleDrawCommand &SetTexture(int virtualTextureID) {
            mVirtualTextureID = virtualTextureID;
            return *this;
        }

        TriangleDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            mTintColor = tintColor;
            return *this;
        }

        TriangleDrawCommand &SetDepth(int overrideDepth) {
            mOverrideDepth = overrideDepth;
            return *this;
        }

        TriangleDrawCommand &SetClipRegionId(int clipRegionId) {
            mClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    private:
        std::array<glm::vec2, 3> mPositions;
        std::array<glm::vec2, 3> mUVs;
        int mVirtualTextureID;
        glm::u8vec4 mTintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        std::optional<int> mOverrideDepth = std::nullopt;
        int mClipRegionId = -1;
    };

    export class QuadDrawCommand : public virtual DrawCommand {
    public:
        QuadDrawCommand() = default;

        QuadDrawCommand &SetPositions(const glm::vec2 &p0,
                                      const glm::vec2 &p2) {
            mFirstPoint = p0;
            mSecondPoint = p2;
            return *this;
        }

        QuadDrawCommand &SetFirstPoint(const glm::vec2 &p0) {
            mFirstPoint = p0;
            return *this;
        }

        QuadDrawCommand &SetSecondPoint(const glm::vec2 &p2) {
            mSecondPoint = p2;
            return *this;
        }

        QuadDrawCommand &SetUVs(const glm::vec2 &uv0,
                                const glm::vec2 &uv1) {
            mFirstUV = uv0;
            mSecondUV = uv1;
            return *this;
        }

        QuadDrawCommand &SetFirstUV(const glm::vec2 &uv0) {
            mFirstUV = uv0;
            return *this;
        }

        QuadDrawCommand &SetSecondUV(const glm::vec2 &uv1) {
            mSecondUV = uv1;
            return *this;
        }

        QuadDrawCommand &SetTexture(int virtualTextureID) {
            mVirtualTextureID = virtualTextureID;
            mRenderingMode = InstanceRenderingMode::Texture;
            return *this;
        }

        QuadDrawCommand &SetFontAtlas(int virtualTextureID, float msdfPixelRange) {
            mVirtualTextureID = virtualTextureID;
            mRenderingMode = InstanceRenderingMode::MSDF;
            mMSDFPixelRange = msdfPixelRange;
            return *this;
        }

        QuadDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            mTintColor = tintColor;
            return *this;
        }

        QuadDrawCommand &SetDepth(int overrideDepth) {
            mOverrideDepth = overrideDepth;
            return *this;
        }

        QuadDrawCommand &SetClipRegionId(int clipRegionId) {
            mClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    private:
        glm::vec2 mFirstPoint;
        glm::vec2 mSecondPoint;

        glm::vec2 mFirstUV = glm::vec2(0.0f, 0.0f);
        glm::vec2 mSecondUV = glm::vec2(1.0f, 1.0f);

        int mVirtualTextureID;
        glm::u8vec4 mTintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        std::optional<int> mOverrideDepth = std::nullopt;
        int mClipRegionId = -1;

        InstanceRenderingMode mRenderingMode = InstanceRenderingMode::Texture;
        float mMSDFPixelRange;
    };

    // No need to provide draw command for line because it is simple enough

    // Now we add circular draw command.
    export class CircularDrawCommand : public virtual DrawCommand {
    public:
        CircularDrawCommand() = default;

        CircularDrawCommand &SetCenter(const glm::vec2 &center) {
            mCenter = center;
            return *this;
        }

        CircularDrawCommand &SetRadii(const glm::vec2 &radii) {
            mRadii = radii;
            return *this;
        }

        CircularDrawCommand &SetRadius(float radius) {
            mRadii = glm::vec2(radius, radius);
            return *this;
        }

        CircularDrawCommand &SetRotation(float rotation) {
            mRotation = rotation;
            return *this;
        }

        CircularDrawCommand &SetStartAngle(float startAngle) {
            mStartAngle = startAngle;
            return *this;
        }

        CircularDrawCommand &SetEndAngle(float endAngle) {
            mEndAngle = endAngle;
            return *this;
        }

        CircularDrawCommand &SetTexture(int virtualTextureID) {
            mVirtualTextureID = virtualTextureID;
            return *this;
        }

        CircularDrawCommand &SetTintColor(const glm::u8vec4 &tintColor) {
            mTintColor = tintColor;
            return *this;
        }

        CircularDrawCommand &SetEdgeSoftness(float edgeSoftness) {
            mEdgeSoftness = edgeSoftness;
            return *this;
        }

        CircularDrawCommand &SetDepth(int overrideDepth) {
            mOverrideDepth = overrideDepth;
            return *this;
        }

        CircularDrawCommand &SetClipRegionId(int clipRegionId) {
            mClipRegionId = clipRegionId;
            return *this;
        }

        friend class Renderer2D;

    private:
        glm::vec2 mCenter;
        glm::vec2 mRadii;
        float mRotation = 0.0f;
        float mInnerScale = 0.0f;
        float mStartAngle = 0.0f;
        float mEndAngle = std::numbers::pi_v<float> * 2.0f;
        int mVirtualTextureID = -1;
        glm::u8vec4 mTintColor = glm::u8vec4(0u, 0u, 0u, 255u);
        float mEdgeSoftness = 1.0f;
        std::optional<int> mOverrideDepth = 0;
        int mClipRegionId = -1;
    };
}
