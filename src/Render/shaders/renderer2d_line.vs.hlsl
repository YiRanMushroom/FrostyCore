cbuffer GlobalConstants : register(b0, space0) {
    float4x4 u_ViewProjectionMatrix;
};

struct VSInput {
    float2 position : POSITION;
    uint color : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

PSInput main(VSInput vertexInput) {
    PSInput pixelInput;

    // Load position
    float2 position = vertexInput.position;

    // Apply color - same decoding as triangle shader
    float4 color = float4(
        ((vertexInput.color >> 24) & 0xFF) / 255.0,
        ((vertexInput.color >> 16) & 0xFF) / 255.0,
        ((vertexInput.color >> 8) & 0xFF) / 255.0,
        (vertexInput.color & 0xFF) / 255.0
    );

    // Transform position to clip space
    float4 clipPosition = mul(u_ViewProjectionMatrix, float4(position, 0.0, 1.0));

    pixelInput.position = clipPosition;
    pixelInput.color = color;

    return pixelInput;
}