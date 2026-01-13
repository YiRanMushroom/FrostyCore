export module Render.FontRenderer;

import Core.Prelude;
import Render.FontResource;
import Render.Renderer2D;

namespace
Engine {
    export struct DrawTextAsciiCommand : Engine::DrawCommand {
        std::string_view Text;
        glm::vec2 StartPosition; // Left top
        glm::vec2 EndPosition; // Right bottom
        glm::vec2 ClipTopLeft;
        glm::vec2 ClipBottomRight;
        glm::u8vec4 Color;
        float FontSize;
        FontAtlasData *Context;
        uint32_t VirtualFontTextureId;

        DrawTextAsciiCommand &SetText(std::string_view text) {
            Text = text;
            return *this;
        }

        DrawTextAsciiCommand &SetPosition(const glm::vec2 &start, const glm::vec2 &end) {
            StartPosition = start;
            EndPosition = end;
            return *this;
        }

        DrawTextAsciiCommand &SetStartPosition(const glm::vec2 &start) {
            StartPosition = start;
            return *this;
        }

        DrawTextAsciiCommand &SetEndPosition(const glm::vec2 &end) {
            EndPosition = end;
            return *this;
        }

        DrawTextAsciiCommand &SetClipRegion(const glm::vec2 &topLeft, const glm::vec2 &bottomRight) {
            ClipTopLeft = topLeft;
            ClipBottomRight = bottomRight;
            return *this;
        }

        DrawTextAsciiCommand &SetColor(const glm::u8vec4 &color) {
            Color = color;
            return *this;
        }

        DrawTextAsciiCommand &SetFontSize(float fontSize) {
            FontSize = fontSize;
            return *this;
        }

        DrawTextAsciiCommand &SetFontContext(FontAtlasData *context) {
            Context = context;
            return *this;
        }

        DrawTextAsciiCommand &SetVirtualFontTextureId(uint32_t textureId) {
            VirtualFontTextureId = textureId;
            return *this;
        }
    };

    export template<>
    void Renderer2D::Draw<DrawTextAsciiCommand>(const DrawTextAsciiCommand &cmd) {
        if (!cmd.Context) return;
        Engine::FontAtlasData &ctx = *cmd.Context;

        // 1. Clipping (Standard Rect)
        Engine::ClipRegion clipRegion;
        clipRegion.ClipMode = Engine::ClipMode::ShowInside;
        clipRegion.Points = {
            cmd.ClipTopLeft,
            {cmd.ClipBottomRight.x, cmd.ClipTopLeft.y},
            cmd.ClipBottomRight,
            {cmd.ClipTopLeft.x, cmd.ClipBottomRight.y}
        };
        clipRegion.PointCount = 4;
        uint32_t clipRegionId = mClipRegionManager.RegisterClipRegion(clipRegion);

        Engine::QuadDrawCommand quadDrawCommand;
        quadDrawCommand.SetFontAtlas(cmd.VirtualFontTextureId, ctx.MTSDFPixelRange)
                .SetTintColor(cmd.Color)
                .SetClipRegionId(clipRegionId);

        float scale = cmd.FontSize;
        float cursorX = cmd.StartPosition.x;
        float cursorY = cmd.StartPosition.y; // Starting Y

        for (char c: cmd.Text) {
            if (c == '\r') continue;

            if (c == '\n') {
                cursorX = cmd.StartPosition.x;
                cursorY += cmd.FontSize; // Y-Down: ADD to move to the NEXT line (Downwards)
                // If we go past the Bottom end position, stop.
                if (cursorY > cmd.EndPosition.y) break;
                continue;
            }

            const Engine::GlyphMetrics *glyphMetrics = ctx.ReadMetricsSafe(static_cast<uint32_t>(c));
            if (!glyphMetrics) continue;

            float advanceWidth = glyphMetrics->Advance * scale;
            if (cursorX != cmd.StartPosition.x && (cursorX + advanceWidth) > cmd.EndPosition.x) {
                cursorX = cmd.StartPosition.x;
                cursorY += cmd.FontSize; // Wrap Downwards
                if (cursorY > cmd.EndPosition.y) break;
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
                quadDrawCommand.SetFirstPoint({x0, y_top})
                        .SetSecondPoint({x1, y_bottom})
                        .SetFirstUV(uv_top_left)
                        .SetSecondUV(uv_bottom_right);

                Draw(quadDrawCommand);
            }
            cursorX += advanceWidth;
        }
    }
}
