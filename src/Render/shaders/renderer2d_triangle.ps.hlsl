Texture2D u_Textures[] : register(t0, space1);
SamplerState u_Sampler  : register(s0, space0);

// Point-in-polygon test using cross product method
bool isPointInPolygon(float2 p, float2 points[4], uint count) {
    bool inside = false;

    for (uint i = 0, j = count - 1; i < count; j = i++) {
        float2 pi = points[i];
        float2 pj = points[j];

        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }

    return inside;
}

struct PSInput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation int textureIndex : TEXCOORD1;
    float2 worldPos : TEXCOORD2;
    nointerpolation float2 clipPoints[4] : CLIP_POINTS;
    nointerpolation uint clipPointCount : CLIP_COUNT;
    nointerpolation uint clipMode : CLIP_MODE;
};

float4 main(PSInput input) : SV_Target {
    // Perform clipping test if enabled (in virtual/world space)
    if (input.clipPointCount > 0) {
        bool inside = isPointInPolygon(input.worldPos, input.clipPoints, input.clipPointCount);

        // clipMode: 0 = show inside (discard outside), 1 = show outside (discard inside)
        if (input.clipMode == 0 && !inside) {
            discard;
        } else if (input.clipMode == 1 && inside) {
            discard;
        }
    }

    float4 outColor = input.tintColor;

    float4 sampledColor;

    if (input.textureIndex >= 0) {
        sampledColor = u_Textures[NonUniformResourceIndex(input.textureIndex)].SampleLevel(u_Sampler, input.texCoord, 0);
    } else {
        sampledColor = float4(1.0, 1.0, 1.0, 1.0);
    }

    outColor *= sampledColor;

    if (outColor.a < 0.001f) {
        discard;
    }

    return outColor;
}