export module Render.TextRenderer;

import Core.Prelude;
import Render.FontResource;
import Render.Renderer2D;

import Core.Utilities;
import "Renderer2D/CommandGenHelper.hpp";
import Render.RenderAPI;

namespace
Engine {
    namespace RenderDrawCommandNames {
        export constexpr StringLiteral DrawSimpleTextAscii = "DrawSimpleTextAscii"_sl;
    }

    export template<>
    class CommandEncoder<RenderDrawCommandNames::DrawSimpleTextAscii> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::DrawSimpleTextAscii)

        EncoderProperty(Text, std::string_view);
        EncoderProperty(StartPosition, glm::vec2);
        EncoderProperty(EndPosition, glm::vec2);
        EncoderProperty(ClipRegionId, int);
        EncoderProperty(EntityID, uint32_t);
        EncoderProperty(Color, glm::u8vec4);
        EncoderProperty(FontSize, float);
        EncoderProperty(Context, FontAtlasData*);
        EncoderProperty(VirtualFontTextureId, uint32_t);
        EncoderProperty(ModelMatrix, glm::mat4);

    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };

    void CommandEncoder<RenderDrawCommandNames::DrawSimpleTextAscii>::EncodeToRenderer(IRenderer2D &renderer) {
        if (renderer.GetRHIAPI() == RHIAPI::NVRHI) {
            auto &nvRenderer = static_cast<NVRenderer2D &>(renderer);
            if (!Context) return;
            Engine::FontAtlasData &ctx = *Context;

            auto quadDrawCommandEncoder
                    = Engine::CommandEncoders::QuadMTSDF{}
                    .SetTextureID(VirtualFontTextureId)
                    .SetTintColor(Color)
                    .SetClipRegionId(ClipRegionId)
                    .SetEntityID(EntityID)
                    .SetMTSDFPixelRange(ctx.MTSDFPixelRange)
                    .SetModelMatrix(ModelMatrix);

            float scale = FontSize;
            float cursorX = StartPosition.x;
            float cursorY = StartPosition.y; // Starting Y

            for (char c: Text) {
                if (c == '\r') continue;

                if (c == '\n') {
                    cursorX = StartPosition.x;
                    cursorY += FontSize; // Y-Down: ADD to move to the NEXT line (Downwards)
                    // If we go past the Bottom end position, stop.
                    if (cursorY > EndPosition.y) break;
                    continue;
                }

                const Engine::GlyphMetrics *glyphMetrics = ctx.ReadMetricsSafe(static_cast<uint32_t>(c));
                if (!glyphMetrics) continue;

                float advanceWidth = glyphMetrics->Advance * scale;
                if (cursorX != StartPosition.x && (cursorX + advanceWidth) > EndPosition.x) {
                    cursorX = StartPosition.x;
                    cursorY += FontSize; // Wrap Downwards
                    if (cursorY > EndPosition.y) break;
                }

                if (c != ' ' && c != '\t') {
                    /* Y-Down Logic:
                       The Baseline is cursorY.
                       In Font Metrics, Y increases Upwards.
                       In Screen Space (Y-Down), we SUBTRACT font-Y from baseline.
                    */
                    float x0 = cursorX + glyphMetrics->Offset.x * scale;
                    float x1 = x0 + glyphMetrics->Size.x * scale;

                    // y_top is "higher" on screen, so it has a SMALLER Y value
                    float y_top = cursorY - (glyphMetrics->Offset.y + glyphMetrics->Size.y) * scale;
                    // y_bottom is "lower" on screen, so it has a LARGER Y value
                    float y_bottom = cursorY - glyphMetrics->Offset.y * scale;

                    /*
                       UV Mapping for Upside-Down Texture:
                       Row 0 in memory is the Foot of the letter.
                       V=0 maps to Row 0 -> Bottom.
                       V=1 maps to Last Row -> Top.
                    */
                    glm::vec2 uv_top_left = {glyphMetrics->BottomLeftUV.x, glyphMetrics->TopRightUV.y};
                    glm::vec2 uv_bottom_right = {glyphMetrics->TopRightUV.x, glyphMetrics->BottomLeftUV.y};
                    // Set P1 as Top-Left, P2 as Bottom-Right (Industry Standard)
                    quadDrawCommandEncoder
                            .SetPositions({{x0, y_top}, {x1, y_bottom}})
                            .SetTextureUVs({uv_top_left, uv_bottom_right});

                    quadDrawCommandEncoder.EncodeToRenderer(nvRenderer);
                }
                cursorX += advanceWidth;
            }
        } else {
            throw RHIIncompatibleException(
                "CommandEncoder<DrawSimpleTextAscii>: Unsupported RHIAPI in EncodeToRenderer");
        }
    }

    namespace CommandEncoders {
        export using SimpleTextAscii = CommandEncoder<RenderDrawCommandNames::DrawSimpleTextAscii>;
    }
}
