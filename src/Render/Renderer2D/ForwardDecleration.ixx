export module Render.Renderer2D:ForwardDecleration;

import Core.Prelude;

namespace Engine {
    export class Renderer2D;
    export struct Renderer2DDescriptor;
    export enum class ClipMode : uint32_t;
    export struct ClipRegion;

    struct TriangleVertexData;
    export enum class InstanceRenderingMode : uint32_t;
    struct TriangleInstanceData;
    export struct TriangleRenderingData;
    struct TriangleRenderingSubmissionData;
    struct TriangleRenderingCommandList;
    struct TriangleBatchRenderingResources;

    struct LineVertexData;
    export struct LineRenderingData;
    struct LineRenderingSubmissionData;
    struct LineRenderingCommandList;
    struct LineBatchRenderingResources;

    struct EllipseShapeData;
    export struct EllipseRenderingData;
    struct EllipseRenderingSubmissionData;
    struct EllipseRenderingCommandList;
    struct EllipseBatchRenderingResources;

}
