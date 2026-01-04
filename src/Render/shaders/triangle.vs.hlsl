struct VS_OUT {
    float4 pos : SV_Position;
    float3 color : COLOR;
};

VS_OUT main(uint vId : SV_VertexID) {
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };

    float3 colors[3] = {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };

    VS_OUT output;
    output.pos = float4(positions[vId], 0.0, 1.0);
    output.color = colors[vId];
    return output;
}