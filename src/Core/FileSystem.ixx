export module Core.FileSystem;

import Core.Prelude;
import Vendor.PlatformAPI;
import Core.Coroutine;

namespace
Engine {
    export Awaitable<std::vector<std::filesystem::path>> OpenFileDialogAsync(
        SDL_Window *parentWindow,
        const std::span<SDL_DialogFileFilter> &filterPatterns) {
        std::promise<std::vector<std::filesystem::path>> *promise = new std::promise<std::vector<
            std::filesystem::path>>{};
        auto future = promise->get_future();
        SDL_DialogFileCallback callback = [](void *userData, const char *const *filePaths, int filterIndex) {
            (void) filterIndex;
            auto promise = std::unique_ptr<std::promise<std::vector<std::filesystem::path>>>
                    (static_cast<std::promise<std::vector<std::filesystem::path>> *>(userData));
            std::vector<std::filesystem::path> paths;
            if (filePaths) {
                for (int i = 0; filePaths[i] != nullptr; ++i) {
                    paths.emplace_back(std::filesystem::path(reinterpret_cast<const char8_t * const>(filePaths[i])));
                }
            }
            promise->set_value(std::move(paths));
        };

        SDL_ShowOpenFileDialog(callback, promise, parentWindow, filterPatterns.data(),
                               static_cast<int>(filterPatterns.size()), nullptr, true);

        co_return co_await std::move(future);
    }

    const std::filesystem::path ExecutablePath = std::filesystem::current_path();

    export const std::filesystem::path &GetExecutablePath() {
        return ExecutablePath;
    }
}
