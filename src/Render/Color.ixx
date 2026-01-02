export module Render.Color;
import Vendor.ApplicationAPI;

namespace Engine::Color {
    export constexpr uint32_t MyBlueHex = 0x37AEFC;
    export constexpr uint32_t MyPinkHex = 0xFFD2DC;
    export constexpr uint32_t MyWhiteHex = 0xFFFFFF;

    export nvrhi::Color ComputeFromHex(uint32_t hexValue) {
        float r = static_cast<float>((hexValue >> 16) & 0xFF) / 255.0f;
        float g = static_cast<float>((hexValue >> 8) & 0xFF) / 255.0f;
        float b = static_cast<float>(hexValue & 0xFF) / 255.0f;
        return nvrhi::Color(r, g, b, 1.0f);
    }

    export const nvrhi::Color MyBlue = ComputeFromHex(MyBlueHex);
    export const nvrhi::Color MyPink = ComputeFromHex(MyPinkHex);
    export const nvrhi::Color MyWhite = ComputeFromHex(MyWhiteHex);
}