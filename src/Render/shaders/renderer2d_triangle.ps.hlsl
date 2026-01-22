Texture2D u_Textures[] : register (t0, space1);
SamplerState u_TextureSamplier : register (s0, space0);
SamplerState u_FontSamplier : register (s1, space0);

struct ClipRegion {
    float2 points[4];
    uint pointCount;
    uint clipMode;
};

StructuredBuffer<ClipRegion> u_ClipBuffer : register (t1, space0);

struct PSInput {
    float4 position: SV_Position;
    float2 texCoord: TEXCOORD0;
    float4 tintColor: COLOR0;
    nointerpolation int textureIndex: TEXCOORD1;
    float2 worldPos: TEXCOORD2;
	nointerpolation uint EntityID: ENTITYID0;
    nointerpolation int clipRegionId: CLIP_REGION_ID;
    nointerpolation float pxRange: PX_RANGE;
    nointerpolation uint mode: RENDER_MODE;
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

struct PSOutput {
    float4 color: SV_TARGET0;
	float4 entityID: SV_TARGET1; // Output as float4 for R8G8B8A8_UNORM (normalized 0.0-1.0)
};

PSOutput main(PSInput input) {
	PSOutput output;
    // 1. Clipping Logic - Early exit if no clipping is needed
    if (input.clipRegionId >= 0) {
        ClipRegion clipRegion = u_ClipBuffer[input.clipRegionId];

        // Only process if there are actual clip points
        if (clipRegion.pointCount > 0 && clipRegion.pointCount <= 4) {
            float2 clipPoints[4];
            for (uint i = 0; i < clipRegion.pointCount; ++i) {
                clipPoints[i] = clipRegion.points[i];
            }

            bool inside = isPointInPolygon(input.worldPos, clipPoints, clipRegion.pointCount);

            // clipMode: 0 = show inside (discard outside), 1 = show outside (discard inside)
            if ((clipRegion.clipMode == 0 && !inside) || (clipRegion.clipMode == 1 && inside)) {
                discard;
            }
        }
    }

    float4 sampledColor = float4(1.0, 1.0, 1.0, 1.0);

    if (input.textureIndex >= 0) {
        if (input.mode == 1) { // MTSDF Mode
            if (input.pxRange <= 0.0f) {
				output.color = float4(0.0, 0.0, 0.0, 1.0);
				output.entityID = 0;
                return output; // Invalid pxRange, return transparent
            }

            float4 mtsdf = u_Textures[NonUniformResourceIndex(input.textureIndex)].Sample(
                       u_FontSamplier, input.texCoord);

            float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);

            // I don't know why but apparently the formula with pxRange does not work well, use fwidth directly instead
            // we also need to explicitly void input.pxRange to avoid compiler optimization breaking the interface

            float opacity = clamp((sd - 0.5) / max(fwidth(sd), 0.0001f) + 0.5, 0.0, 1.0);

            sampledColor = float4(1.0, 1.0, 1.0, opacity);
        }
        else { // Standard Texture Mode
            sampledColor = u_Textures[NonUniformResourceIndex(input.textureIndex)].SampleLevel(
                u_TextureSamplier, input.texCoord, 0);
        }
    }

    // return sampledColor; // For debugging

    // Apply Tint Color (TintColor.a affects MSDF opacity)
    float4 outColor = input.tintColor * sampledColor;

    // Alpha Test: discard fragments that are nearly transparent
    if (outColor.a < 0.001f) {
        discard;
    }

	output.color = outColor;

	// Encode EntityID into R8G8B8A8_UNORM format (float4 normalized to 0.0-1.0)
	// EntityID is left-shifted by 8 bits to pack into RGB channels
	// Alpha channel: 0.0 = preserve old value (via blending), 1.0 = write new value
	// Note: EntityID must not exceed 2^24 - 1 (16,777,215)
	if (input.EntityID == 0) {
		// EntityID is 0: set alpha to 0.0 to preserve the previous value via blending
		output.entityID = float4(0.0, 0.0, 0.0, 0.0);
	} else {
		// Encode EntityID: shift left by 8 bits and pack into RGB
		uint encodedID = input.EntityID << 8;
		float r = float((encodedID >> 16) & 0xFF) / 255.0;
		float g = float((encodedID >> 8) & 0xFF) / 255.0;
		float b = float(encodedID & 0xFF) / 255.0;
		output.entityID = float4(r, g, b, 1.0); // alpha = 1.0 to write the value
	}

    return output;
}