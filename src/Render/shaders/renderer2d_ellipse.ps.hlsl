Texture2D u_Textures[] : register(t0, space1);
SamplerState u_Sampler  : register(s0, space0);

static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;

struct PSInput {
    float4 position : SV_POSITION;
    float2 worldPos : TEXCOORD0;
    float4 tintColor : COLOR0;
    nointerpolation float2 center : CENTER;
    nointerpolation float2 radii : RADII;
    nointerpolation float rotation : ROTATION;
    nointerpolation float innerScale : INNER_SCALE;
    nointerpolation float startAngle : ANGLE_START;
    nointerpolation float endAngle : ANGLE_END;
    nointerpolation int textureIndex : TEXCOORD1;
    nointerpolation float edgeSoftness : EDGE_SOFTNESS;
};

float ellipseSDF(float2 p, float2 radii) {
    float2 pn = p / radii;
    float len = length(pn);

    float dist = (len - 1.0) * min(radii.x, radii.y);
    return dist;
}

float normalizeAngle(float angle) {
    angle = fmod(angle + PI, TWO_PI);
    if (angle < 0.0) angle += TWO_PI;
    return angle - PI;
}

float checkAngleInSector(float angle, float startAngle, float endAngle, float softness) {
    float start = normalizeAngle(startAngle);
    float end = normalizeAngle(endAngle);
    float a = normalizeAngle(angle);

    float inSector;

    if (start <= end) {
        inSector = smoothstep(start - softness, start + softness, a) *
                   smoothstep(end + softness, end - softness, a);
    } else {
        float inFirst = smoothstep(start - softness, start + softness, a);
        float inSecond = smoothstep(end + softness, end - softness, a);
        inSector = max(inFirst, 1.0 - inSecond);
    }

    return inSector;
}

float4 main(PSInput input) : SV_TARGET {
    float2 offset = input.worldPos - input.center;

    float cosR = cos(-input.rotation);
    float sinR = sin(-input.rotation);
    float2x2 invRotMatrix = float2x2(cosR, -sinR, sinR, cosR);
    float2 localPos = mul(invRotMatrix, offset);

    float dist = ellipseSDF(localPos, input.radii);

    float2 normalizedPos = localPos / input.radii;
    float distToCenter = length(normalizedPos);

    float effectiveRadius = min(input.radii.x, input.radii.y);
    if (distToCenter > 0.001) {
        float2 direction = normalizedPos / distToCenter;
        float cosTheta = direction.x;
        float sinTheta = direction.y;
        effectiveRadius = (input.radii.x * input.radii.y) /
                         sqrt(input.radii.y * input.radii.y * cosTheta * cosTheta +
                              input.radii.x * input.radii.x * sinTheta * sinTheta);
    }

    float avgRadius = (input.radii.x + input.radii.y) * 0.5;
    float radiusRatio = effectiveRadius / avgRadius;

    float fw = fwidth(dist) / radiusRatio;
    fw = max(fw, input.edgeSoftness);
    if (fw < 0.5) fw = 0.5;

    float alpha = smoothstep(fw, -fw, dist);

    if (input.innerScale > 0.001) {
        float2 innerRadii = input.radii * input.innerScale;
        float innerDist = ellipseSDF(localPos, innerRadii);
        alpha *= smoothstep(-fw, fw, innerDist);
    }

    float angleDiff = abs(input.endAngle - input.startAngle);
    if (angleDiff > 0.001 && angleDiff < TWO_PI - 0.001) {
        float angle = atan2(localPos.y, localPos.x);

        float maxR = max(input.radii.x, input.radii.y);
        float angleSoftness = fw / maxR;

        float start = input.startAngle;
        float end = input.endAngle;

        if (start > end) {
            if (angle < start && angle > end) {
                alpha *= smoothstep(0.0, angleSoftness, min(start - angle, angle - end));
                alpha = 0.0;
            }
        } else {
            if (angle < start) {
                alpha *= smoothstep(0.0, angleSoftness, start - angle);
                alpha = 0.0;
            }
            if (angle > end) {
                alpha *= smoothstep(0.0, angleSoftness, angle - end);
                alpha = 0.0;
            }
        }
    }

    float4 outColor = input.tintColor;

    if (input.textureIndex >= 0) {
        float2 uv = localPos / (input.radii * 2.0) + 0.5;
        float4 sampled = u_Textures[NonUniformResourceIndex(input.textureIndex)].Sample(u_Sampler, uv);
        outColor *= sampled;
    }

    outColor.a *= alpha;

    if (outColor.a < 0.001) {
        discard;
    }

    return outColor;
}