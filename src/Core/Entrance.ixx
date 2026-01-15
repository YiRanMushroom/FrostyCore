export module Core.Entrance;

#if FrostyDefineMain

import Core.Exception;
import std;
import "SDL3/SDL.h";
import Core.Coroutine;

namespace
Engine {
    export extern "C++" int Main(int argc, char **argv);
}

export int main(int argc, char **argv) {
    int result = -1;

    try {
        Engine::RegisterSystemFatalExceptionHandler();
        Engine::InitializeGlobalExecutionContext();

        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return -1;
        }


        result = Engine::Main(argc, argv);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit();

        Engine::DestroyGlobalExecutionContext();
    } catch (std::exception &ex) {
        std::cerr << "Uncaught exception: " << ex.what() << std::endl;
    }
    return result;
}

#endif
