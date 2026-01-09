cbuffer GlobalConstants : register(b0, space0) {
    float4x4 u_ViewProjectionMatrix;
};

struct EllipseShapeData {
    float2 center;
    float2 radii;
    float rotation;
    float innerScale;
    float startAngle;
    float endAngle;
    uint tintColor;
    int textureIndex;
    float edgeSoftness;
    float _padding;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 worldPos : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation float2 center : CENTER;
    nointerpolation float2 radii : RADII;
    nointerpolation float rotation : ROTATION;
    nointerpolation float innerScale : INNER_SCALE;
    nointerpolation float startAngle : ANGLE_START;
    nointerpolation float endAngle : ANGLE_END;
    nointerpolation int textureIndex : TEXCOORD1;
    nointerpolation float edgeSoftness : EDGE_SOFTNESS;
};

StructuredBuffer<EllipseShapeData> u_ShapeBuffer : register(t0, space0);

static const float2 kQuadVertices[6] = {
    float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), // Triangle 1
    float2(-1.0, 1.0),  float2(1.0, -1.0), float2(1.0, 1.0)   // Triangle 2
};

PSInput main(uint vID : SV_VertexID) {
    PSInput output;

    uint shapeIndex = vID / 6;
    uint vertexInShape = vID % 6;

    EllipseShapeData data = u_ShapeBuffer[shapeIndex];

    float maxRadius = max(data.radii.x, data.radii.y);
    float boundingRadius = maxRadius * 1.05;

    float2 localPos = kQuadVertices[vertexInShape] * boundingRadius;

    float cosR = cos(data.rotation);
    float sinR = sin(data.rotation);
    float2x2 rotMatrix = float2x2(cosR, -sinR, sinR, cosR);
    float2 rotatedPos = mul(rotMatrix, localPos);

    float2 worldPos = data.center + rotatedPos;

    output.position = mul(u_ViewProjectionMatrix, float4(worldPos, 0.0, 1.0));
    output.worldPos = worldPos;
    output.center = data.center;
    output.radii = data.radii;
    output.rotation = data.rotation;
    output.innerScale = data.innerScale;
    output.startAngle = data.startAngle;
    output.endAngle = data.endAngle;
    output.textureIndex = data.textureIndex;
    output.edgeSoftness = data.edgeSoftness;

    output.tintColor = float4(
        ((data.tintColor >> 24) & 0xFF) / 255.0,
        ((data.tintColor >> 16) & 0xFF) / 255.0,
        ((data.tintColor >> 8) & 0xFF) / 255.0,
        (data.tintColor & 0xFF) / 255.0
    );

    return output;
}