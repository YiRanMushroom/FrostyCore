export module Render.Renderer2D:DrawCommands;

import :ForwardDecleration;
import :Misc;
import :TriangleAndQuad;
import :Line;
import :Eclipse;
import Core.Prelude;

import Core.Utilities;

import "CommandGenHelper.hpp";

namespace
Engine {
    export class IRenderer2D;

    export class ICommandEncoder {
    public:
        virtual ~ICommandEncoder() = default;

        virtual void EncodeToRenderer(IRenderer2D &) = 0;
    };


    export template<StringLiteral CommandName>
    class CommandEncoder : public ICommandEncoder {
        // must be specialized
    };

    namespace RenderDrawCommandNames {
        export constexpr StringLiteral Triangle = "Triangle"_sl;
        export constexpr StringLiteral QuadTexture = "QuadTexture"_sl;
        export constexpr StringLiteral QuadMTSDF = "QuadMTSDF"_sl;
        export constexpr StringLiteral Circular = "Circular"_sl;
        export constexpr StringLiteral ThinLine = "ThinLine"_sl;
    }

    export template<>
    class CommandEncoder<RenderDrawCommandNames::Triangle> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::Triangle)

        EncoderProperty(Positions, glm::mat3x2);
        EncoderProperty(TextureUVs, glm::mat3x2);
        EncoderPropertyOptional(TextureID, uint32_t);
        EncoderPropertyOptional(TintColor, glm::u8vec4);
        EncoderPropertyOptional(Depth, int);
        EncoderPropertyOptional(ClipRegionId, uint32_t);
        EncoderPropertyOptional(EntityID, uint32_t);
        EncoderPropertyOptional(ModelMatrix, glm::mat4);

    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };

    export template<>
    class CommandEncoder<RenderDrawCommandNames::QuadTexture> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::QuadTexture)

        EncoderProperty(Positions, glm::mat2x2);
        EncoderProperty(TextureUVs, glm::mat2x2);
        EncoderPropertyOptional(TextureID, uint32_t);
        EncoderPropertyOptional(TintColor, glm::u8vec4);
        EncoderPropertyOptional(Depth, int);
        EncoderPropertyOptional(ClipRegionId, uint32_t);
        EncoderPropertyOptional(EntityID, uint32_t);
        EncoderPropertyOptional(ModelMatrix, glm::mat4);
    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };

    export template<>
    class CommandEncoder<RenderDrawCommandNames::QuadMTSDF> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::QuadMTSDF)

        EncoderProperty(Positions, glm::mat2x2);
        EncoderProperty(TextureUVs, glm::mat2x2);
        EncoderPropertyOptional(TextureID, uint32_t);
        EncoderPropertyOptional(MTSDFPixelRange, float);
        EncoderPropertyOptional(TintColor, glm::u8vec4);
        EncoderPropertyOptional(Depth, int);
        EncoderPropertyOptional(ClipRegionId, uint32_t);
        EncoderPropertyOptional(EntityID, uint32_t);
        EncoderPropertyOptional(ModelMatrix, glm::mat4);
    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };

    export template<>
    class CommandEncoder<RenderDrawCommandNames::Circular> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::Circular)

        EncoderProperty(Center, glm::vec2);
        EncoderProperty(Radii, glm::vec2);
        EncoderProperty(Rotation, float);
        EncoderProperty(StartAngle, float);
        EncoderProperty(EndAngle, float);
        EncoderPropertyOptional(TextureID, uint32_t);
        EncoderPropertyOptional(TintColor, glm::u8vec4);
        EncoderPropertyOptional(EdgeSoftness, float);
        EncoderPropertyOptional(Depth, int);
        EncoderPropertyOptional(ClipRegionId, uint32_t);
        EncoderPropertyOptional(EntityID, uint32_t);
        EncoderPropertyOptional(ModelMatrix, glm::mat4);
    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };

    export template<>
    class CommandEncoder<RenderDrawCommandNames::ThinLine> : public ICommandEncoder {
        GenCommandEncoderBase(RenderDrawCommandNames::ThinLine)

        EncoderProperty(Positions, glm::mat2x2);
        EncoderPropertyOptional(Color, glm::u8vec4);

    public:
        void EncodeToRenderer(IRenderer2D &) override;
    };


    namespace CommandEncoders {
        export using Triangle = CommandEncoder<RenderDrawCommandNames::Triangle>;
        export using QuadTexture = CommandEncoder<RenderDrawCommandNames::QuadTexture>;
        export using QuadMTSDF = CommandEncoder<RenderDrawCommandNames::QuadMTSDF>;
        export using Circular = CommandEncoder<RenderDrawCommandNames::Circular>;
    }
}
