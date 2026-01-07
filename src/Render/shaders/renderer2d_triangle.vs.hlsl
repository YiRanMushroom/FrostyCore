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
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation int textureIndex : TEXCOORD1;
};

StructuredBuffer<SpriteData> u_SpriteData : register(t0, space0);

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

    return pixelInput;
}