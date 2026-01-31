export module Render.Color;
import Vendor.ApplicationAPI;
import Core.Prelude;

namespace Engine::Color {
    export constexpr uint32_t MyBlueHex = 0x37AEFC;
    export constexpr uint32_t MyPinkHex = 0xFFD2DC;
    export constexpr uint32_t MyWhiteHex = 0xFFFFFF;

    export constexpr glm::u8vec4 ComputeFromHex(uint32_t hexValue) {
        uint8_t r = (hexValue >> 16) & 0xFF;
        uint8_t g = (hexValue >> 8) & 0xFF;
        uint8_t b = hexValue & 0xFF;
        return glm::u8vec4(r, g, b, 255u);
    }

    export constexpr glm::u8vec4 MyBlue = ComputeFromHex(MyBlueHex);
    export constexpr glm::u8vec4 MyPink = ComputeFromHex(MyPinkHex);
    export constexpr glm::u8vec4 MyWhite = ComputeFromHex(MyWhiteHex);
}
