export module Core.Entrance;

#define FrostyDefineMain 1

#if FrostyDefineMain

import Core.Exception;
import std;

namespace Engine {
    export extern "C++" int Main(int argc, char** argv);
}

export int main(int argc, char** argv) {
    Engine::RegisterSystemFatalExceptionHandler();
    try {
        return Engine::Main(argc, argv);
    } catch (std::exception &ex) {
        std::cerr << "Uncaught exception: " << ex.what() << std::endl;
        return -1;
    }
}

#endif
