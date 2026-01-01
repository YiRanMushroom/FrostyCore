export module Core.Entrance;

#if FrostyDefineMain

import Core.Exception;
import std;
import "SDL3/SDL.h";

namespace
Engine {
    export extern "C++" int Main(int argc, char **argv);
}

export int main(int argc, char **argv) {
    Engine::RegisterSystemFatalExceptionHandler();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    try {
        return Engine::Main(argc, argv);
    } catch (std::exception &ex) {
        std::cerr << "Uncaught exception: " << ex.what() << std::endl;
    }

    SDL_Quit();

    return -1;
}

#endif
