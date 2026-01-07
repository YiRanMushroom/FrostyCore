Texture2D u_Textures[] : register(t0, space1);
SamplerState u_Sampler  : register(s0, space0);

struct PSInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation int textureIndex : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target {
    float4 outColor = input.tintColor;

    float4 sampledColor;

    if (input.textureIndex >= 0) {
        sampledColor = u_Textures[input.textureIndex].SampleLevel(u_Sampler, input.texCoord, 0);
    } else {
        sampledColor = float4(1.0, 1.0, 1.0, 1.0);
    }

    outColor *= sampledColor;

    if (outColor.a < 0.001f) {
        discard;
    }

    return outColor;
}