module Render.Renderer2D;

import :ForwardDecleration;
import :Misc;
import :TriangleAndQuad;
import :Line;
import :Eclipse;
import :Renderer;
import Core.Prelude;

import Render.RenderAPI;

namespace
Engine {
    void CommandEncoder<RenderDrawCommandNames::Triangle>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            nvRenderer.mTriangleCommandList.Instances.push_back(
                TriangleRenderingData{
                    .Positions = glm::mat4x2(Positions[0], Positions[1], Positions[2],
                                             glm::vec2(0.0f, 0.0f)),
                    .TexCoords = glm::mat4x2(TextureUVs[0], TextureUVs[1], TextureUVs[2], glm::vec2(0.0f, 0.0f)),
                    .IsQuad = false,
                    .VirtualTextureID = TextureID ? static_cast<int>(TextureID.value()) : -1,
                    .TintColor = TintColor.value_or(glm::u8vec4(0u, 0u, 0u, 255u)),
                    .Depth = Depth.value_or(nvRenderer.mCurrentDepth),
                    .ClipRegionId = ClipRegionId ? static_cast<int>(*ClipRegionId) : -1,
                    .EntityID = EntityID.value_or(0),
                    .RenderingMode = InstanceRenderingMode::Texture,
                    .MTSDFPixelRange = 0.0f,
                    .ModelMatrix = ModelMatrix.value_or(glm::mat4(1.0f))
                }
            );
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<Triangle>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }

    void CommandEncoder<RenderDrawCommandNames::QuadTexture>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            nvRenderer.mTriangleCommandList.Instances.push_back(
                TriangleRenderingData{
                    .Positions = glm::mat4x2(
                        glm::vec2(Positions[0].x, Positions[0].y),
                        glm::vec2(Positions[1].x, Positions[0].y),
                        glm::vec2(Positions[1].x, Positions[1].y),
                        glm::vec2(Positions[0].x, Positions[1].y)
                    ),
                    .TexCoords = glm::mat4x2(
                        glm::vec2(TextureUVs[0].x, TextureUVs[0].y),
                        glm::vec2(TextureUVs[1].x, TextureUVs[0].y),
                        glm::vec2(TextureUVs[1].x, TextureUVs[1].y),
                        glm::vec2(TextureUVs[0].x, TextureUVs[1].y)
                    ),
                    .IsQuad = true,
                    .VirtualTextureID = TextureID ? static_cast<int>(TextureID.value()) : -1,
                    .TintColor = TintColor.value_or(glm::u8vec4(0u, 0u, 0u, 255u)),
                    .Depth = Depth.value_or(nvRenderer.mCurrentDepth),
                    .ClipRegionId = ClipRegionId ? static_cast<int>(*ClipRegionId) : -1,
                    .EntityID = EntityID.value_or(0),
                    .RenderingMode = InstanceRenderingMode::Texture,
                    .MTSDFPixelRange = 0.0f,
                    .ModelMatrix = ModelMatrix.value_or(glm::mat4(1.0f))
                }
            );
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<QuadTexture>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }

    void CommandEncoder<RenderDrawCommandNames::QuadMTSDF>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            nvRenderer.mTriangleCommandList.Instances.push_back(
                TriangleRenderingData{
                    .Positions = glm::mat4x2(
                        glm::vec2(Positions[0].x, Positions[0].y),
                        glm::vec2(Positions[1].x, Positions[0].y),
                        glm::vec2(Positions[1].x, Positions[1].y),
                        glm::vec2(Positions[0].x, Positions[1].y)
                    ),
                    .TexCoords = glm::mat4x2(
                        glm::vec2(TextureUVs[0].x, TextureUVs[0].y),
                        glm::vec2(TextureUVs[1].x, TextureUVs[0].y),
                        glm::vec2(TextureUVs[1].x, TextureUVs[1].y),
                        glm::vec2(TextureUVs[0].x, TextureUVs[1].y)
                    ),
                    .IsQuad = true,
                    .VirtualTextureID = TextureID ? static_cast<int>(TextureID.value()) : -1,
                    .TintColor = TintColor.value(),
                    .Depth = Depth.value_or(nvRenderer.mCurrentDepth),
                    .ClipRegionId = ClipRegionId ? static_cast<int>(*ClipRegionId) : -1,
                    .EntityID = EntityID.value_or(0),
                    .RenderingMode = InstanceRenderingMode::MTSDF,
                    .MTSDFPixelRange = MTSDFPixelRange.value(),
                    .ModelMatrix = ModelMatrix.value_or(glm::mat4(1.0f))
                }
            );
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<QuadMTSDF>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }

    void CommandEncoder<RenderDrawCommandNames::Circular>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            nvRenderer.mEllipseCommandList.Instances.push_back(
                EllipseRenderingData{
                    .Center = Center,
                    .Radii = Radii,
                    .VirtualTextureID = TextureID ? static_cast<int>(TextureID.value()) : -1,
                    .TintColor = TintColor.value_or(glm::u8vec4(0u, 0u, 0u, 255u)),
                    .Depth = Depth.value_or(nvRenderer.mCurrentDepth),
                    .ClipRegionId = ClipRegionId ? static_cast<int>(*ClipRegionId) : -1,
                    .EntityID = EntityID.value_or(0),
                    .ModelMatrix = ModelMatrix.value_or(glm::mat4(1.0f))
                }
            );
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<Circular>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }

    void CommandEncoder<RenderDrawCommandNames::ThinLine>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            nvRenderer.mLineCommandList.RenderingData.emplace_back(
                LineRenderingData{
                    .Positions = Positions,
                    .Color = Color.value_or(glm::u8vec4(0u, 0u, 0u, 255u))
                }
            );
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<ThinLine>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }
}
