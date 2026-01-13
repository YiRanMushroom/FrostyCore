Texture2D t_Source : register (t0);
SamplerState s_Sampler : register (s0);

struct PSInput {
    float4 pos: SV_Position;
    float2 uv: TEXCOORD;
};

float4 main(PSInput input) : SV_Target{
    float4 color = t_Source.Sample(s_Sampler, input.uv);

    // We have problems, directly output the uv to see what's going on
    // float4 color = float4(input.uv, 0.0f, 1.0f);

    return color;
}