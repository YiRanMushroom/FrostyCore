cbuffer GlobalConstants : register(b0, space0) {
    float4x4 u_ViewProjectionMatrix;
};

struct VSInput {
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    uint constantIndex : CONSTANTINDEX0;
};

struct SpriteData {
    uint tintColor;
    int textureIndex;
    int clipIndex;
    float pxRange;
    uint mode;
    float padding[3];
};

struct ClipRegion {
    float2 points[4];
    uint pointCount;
    uint clipMode;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation int textureIndex : TEXCOORD1;
    float2 worldPos : TEXCOORD2;
    nointerpolation float2 clipPoints[4] : CLIP_POINTS;
    nointerpolation uint clipPointCount : CLIP_COUNT;
    nointerpolation uint clipMode : CLIP_MODE;
    nointerpolation float pxRange : PX_RANGE;
    nointerpolation uint mode : RENDER_MODE;
};

StructuredBuffer<SpriteData> u_SpriteData : register(t0, space0);
StructuredBuffer<ClipRegion> u_ClipBuffer : register(t1, space0);

PSInput main(VSInput vertexInput) {
    PSInput pixelInput;
    SpriteData sprite = u_SpriteData[vertexInput.constantIndex];

    float4 tintColor = float4(
        ((sprite.tintColor >> 24) & 0xFF) / 255.0,
        ((sprite.tintColor >> 16) & 0xFF) / 255.0,
        ((sprite.tintColor >> 8) & 0xFF) / 255.0,
        (sprite.tintColor & 0xFF) / 255.0
    );

    pixelInput.position = mul(u_ViewProjectionMatrix, float4(vertexInput.position, 0.0, 1.0));
    pixelInput.texCoord = vertexInput.texCoord;
    pixelInput.tintColor = tintColor;
    pixelInput.textureIndex = sprite.textureIndex;
    pixelInput.worldPos = vertexInput.position;
    pixelInput.pxRange = sprite.pxRange;
    pixelInput.mode = sprite.mode;

    if (sprite.clipIndex >= 0) {
        ClipRegion clipRegion = u_ClipBuffer[sprite.clipIndex];
        pixelInput.clipPointCount = clipRegion.pointCount;
        pixelInput.clipMode = clipRegion.clipMode;
        for (uint i = 0; i < 4; ++i) {
            pixelInput.clipPoints[i] = (i < clipRegion.pointCount) ? clipRegion.points[i] : float2(0, 0);
        }
    } else {
        pixelInput.clipPointCount = 0;
        pixelInput.clipMode = 0;
    }

    return pixelInput;
}