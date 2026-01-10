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
    int textureIndex; // if < 0, no texture
    int clipIndex;    // if < 0, no clipping
};

struct ClipRegion {
    float2 points[4]; // in virtual/world space (NOT transformed)
    uint pointCount;  // 3 or 4
    uint clipMode;    // 0 = show inside, 1 = show outside
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
};

StructuredBuffer<SpriteData> u_SpriteData : register(t0, space0);
StructuredBuffer<ClipRegion> u_ClipBuffer : register(t1, space0);

PSInput main(VSInput vertexInput) {
    PSInput pixelInput;

    SpriteData sprite = u_SpriteData[vertexInput.constantIndex];

    // Load position and texture coordinates
    float2 position = vertexInput.position;
    float2 texCoord = vertexInput.texCoord;

    // Apply tint color
    float4 tintColor = float4(
        ((sprite.tintColor >> 24) & 0xFF) / 255.0,
        ((sprite.tintColor >> 16) & 0xFF) / 255.0,
        ((sprite.tintColor >> 8) & 0xFF) / 255.0,
        (sprite.tintColor & 0xFF) / 255.0
    );

    // Transform position to clip space
    float4 clipPosition = mul(u_ViewProjectionMatrix, float4(position, 0.0, 1.0));

    pixelInput.position = clipPosition;
    pixelInput.texCoord = texCoord;
    pixelInput.tintColor = tintColor;
    pixelInput.textureIndex = sprite.textureIndex;
    pixelInput.worldPos = position;

    // Load clip region (keep in virtual/world space, no transformation needed)
    if (sprite.clipIndex >= 0) {
        ClipRegion clipRegion = u_ClipBuffer[sprite.clipIndex];
        pixelInput.clipPointCount = clipRegion.pointCount;
        pixelInput.clipMode = clipRegion.clipMode;

        // Pass clip points directly (they're already in virtual space)
        for (uint i = 0; i < 4; ++i) {
            if (i < clipRegion.pointCount) {
                pixelInput.clipPoints[i] = clipRegion.points[i];
            } else {
                pixelInput.clipPoints[i] = float2(0.0, 0.0);
            }
        }
    } else {
        pixelInput.clipPointCount = 0;
        pixelInput.clipMode = 0;
        pixelInput.clipPoints[0] = float2(0.0, 0.0);
        pixelInput.clipPoints[1] = float2(0.0, 0.0);
        pixelInput.clipPoints[2] = float2(0.0, 0.0);
        pixelInput.clipPoints[3] = float2(0.0, 0.0);
    }

    return pixelInput;
}