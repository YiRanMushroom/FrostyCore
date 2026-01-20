export module Core.Input;

import Vendor.ApplicationAPI;

namespace Engine {
    export bool IsKeyPressed(SDL_Scancode key) {
        const bool *state = SDL_GetKeyboardState(nullptr);
        return state[key] != 0;
    }

    export SDL_Keymod GetKeyModifiers() {
        return SDL_GetModState();
    }

    export bool IsMouseButtonPressed(Uint8 button) {
        Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
        return (buttons & SDL_MouseButtonFlags(button)) != 0;
    }
}