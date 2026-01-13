cbuffer GlobalConstants : register (b0, space0) {
    float4x4 u_ViewProjectionMatrix;
};

struct VSInput {
    float2 position: POSITION;
    float2 texCoord: TEXCOORD0;
    uint constantIndex: CONSTANTINDEX0;
};

struct SpriteData {
    uint tintColor;
    int textureIndex;
    int clipRegionId;
    uint mode;
    float pxRange;
    float padding[3];
};

struct PSInput {
    float4 position: SV_POSITION;
    float2 texCoord: TEXCOORD0;
    float4 tintColor: COLOR0;
    nointerpolation int textureIndex: TEXCOORD1;
    float2 worldPos: TEXCOORD2;
    nointerpolation int clipRegionId: CLIP_REGION_ID;
    nointerpolation float pxRange: PX_RANGE;
    nointerpolation uint mode: RENDER_MODE;
};

StructuredBuffer<SpriteData> u_SpriteData : register (t0, space0);

PSInput main(VSInput vertexInput) {
    PSInput pixelInput;
    SpriteData sprite = u_SpriteData[vertexInput.constantIndex];

    uint packed = sprite.tintColor;
    float4 tintColor = float4(
        ((packed >> 24) & 0xFF) / 255.0,
        ((packed >> 16) & 0xFF) / 255.0,
        ((packed >> 8) & 0xFF) / 255.0,
        (packed & 0xFF) / 255.0
    );

    pixelInput.position = mul(u_ViewProjectionMatrix, float4(vertexInput.position, 0.0, 1.0));
    pixelInput.texCoord = vertexInput.texCoord;
    pixelInput.tintColor = tintColor;
    pixelInput.textureIndex = sprite.textureIndex;
    pixelInput.worldPos = vertexInput.position;
    pixelInput.mode = sprite.mode;
    pixelInput.pxRange = sprite.pxRange;
    pixelInput.clipRegionId = sprite.clipRegionId;

    return pixelInput;
}