Texture2D u_Textures[]          : register(t0, space1);
SamplerState u_TextureSamplier  : register(s0, space0);
SamplerState u_FontSamplier     : register(s1, space0);

struct PSInput {
    float4 position                 : SV_Position;
    float2 texCoord                 : TEXCOORD0;
    float4 tintColor                : COLOR0;
    nointerpolation int textureIndex : TEXCOORD1;
    float2 worldPos                 : TEXCOORD2;
    nointerpolation float2 clipPoints[4] : CLIP_POINTS;
    nointerpolation uint clipPointCount : CLIP_COUNT;
    nointerpolation uint clipMode   : CLIP_MODE;
    nointerpolation float pxRange   : PX_RANGE;
    nointerpolation uint mode       : RENDER_MODE;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

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

float4 main(PSInput input) : SV_Target {
    // 1. Clipping Logic (Keep it as is, but be careful of performance)
    if (input.clipPointCount > 0) {
        bool inside = isPointInPolygon(input.worldPos, input.clipPoints, input.clipPointCount);
        if ((input.clipMode == 0 && !inside) || (input.clipMode == 1 && inside)) {
            discard;
        }
    }

    float4 sampledColor = float4(1.0, 1.0, 1.0, 1.0);

    if (input.textureIndex >= 0) {
        if (input.mode == 1) { // MSDF Mode
            float3 msd = u_Textures[NonUniformResourceIndex(input.textureIndex)].Sample(u_FontSamplier, input.texCoord).rgb;
            float sd = median(msd.r, msd.g, msd.b);

            // Standard MSDF formula: convert distance field to screen space
            // fwidth returns the sum of absolute derivatives (approximation of gradient magnitude)
            float screenPxDistance = input.pxRange * (sd - 0.5) / length(fwidth(input.texCoord));

            float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

            sampledColor = float4(1.0, 1.0, 1.0, opacity);
        }
        else { // Standard Texture Mode
            sampledColor = u_Textures[NonUniformResourceIndex(input.textureIndex)].SampleLevel(u_TextureSamplier, input.texCoord, 0);
        }
    }

    // Apply Tint Color (TintColor.a affects MSDF opacity)
    float4 outColor = input.tintColor * sampledColor;

    // Alpha Test: discard fragments that are nearly transparent
    if (outColor.a < 0.001f) {
        discard;
    }

    return outColor;
}