export module Core.FileSystem;

import Core.Prelude;
import Vendor.PlatformAPI;
import Core.Coroutine;
import Core.Utilities;

namespace
Engine {
    const std::filesystem::path ThisExecutablePath = std::filesystem::current_path();

    export const std::filesystem::path &GetThisExecutablePath() {
        return ThisExecutablePath;
    }

    export struct DialogFileFilterPatternElement {
        std::string name;
        std::string pattern;
    };

    export class IDialogFileFilterGroup : public Engine::RefCounted {
    public:
        virtual std::span<const SDL_DialogFileFilter> GetSDLDialogFileFilters() const = 0;
    };

    template<typename ElementType>
    concept DialogFileFilterElement = requires(ElementType element) {
        { std::string(element.name) } -> std::same_as<std::string>;
        { std::string(element.pattern) } -> std::same_as<std::string>;
    };

    export class DialogFileFilterGroup : public IDialogFileFilterGroup {
    public:
        virtual ~DialogFileFilterGroup() override = default;

        DialogFileFilterGroup(std::span<const SDL_DialogFileFilter> filters, ResourceOwnership::Static)
            : mFilters(filters.begin(), filters.end()) {}

        DialogFileFilterGroup(std::span<const SDL_DialogFileFilter> filters, const Ref<RefCounted> &resourceHolder,
                              ResourceOwnership::Shared)
            : mFilters(filters.begin(), filters.end()), mResourceHolder(resourceHolder.As<RefCounted>()) {}


        template<DialogFileFilterElement ElementType>
        DialogFileFilterGroup(std::initializer_list<const ElementType> filters,
                              ResourceOwnership::AutoManaged) {
            mFilters.reserve(filters.size());
            auto resourceHolder = MakeRef<RefInterface<std::vector<std::string>>>();
            mResourceHolder = resourceHolder.As<RefCounted>();
            std::vector<std::string> &ownedStrings = *resourceHolder.Get();
            ownedStrings.reserve(filters.size() * 2);
            for (const auto &filter: filters) {
                auto nameStr = std::string(filter.name);
                ownedStrings.push_back(std::move(nameStr));
                auto patternStr = std::string(filter.pattern);
                ownedStrings.push_back(std::move(patternStr));
                SDL_DialogFileFilter sdlFilter;
                sdlFilter.name = ownedStrings[ownedStrings.size() - 2].c_str();
                sdlFilter.pattern = ownedStrings[ownedStrings.size() - 1].c_str();
                mFilters.push_back(sdlFilter);
            }
        }

        template<DialogFileFilterElement ElementType>
        DialogFileFilterGroup(std::initializer_list<ElementType> filters,
                              ResourceOwnership::Transferred) {
            mFilters.reserve(filters.size());
            auto resourceHolder = MakeRef<RefInterface<std::vector<std::string>>>();
            mResourceHolder = resourceHolder.As<RefCounted>();
            std::vector<std::string> &ownedStrings = *resourceHolder.Get();
            ownedStrings.reserve(filters.size() * 2);

            for (auto &&filter: filters) {
                auto nameStr = std::string(std::move(filter.name));
                ownedStrings.push_back(std::move(nameStr));
                auto patternStr = std::string(std::move(filter.pattern));
                ownedStrings.push_back(std::move(patternStr));
                SDL_DialogFileFilter sdlFilter;
                sdlFilter.name = ownedStrings[ownedStrings.size() - 2].c_str();
                sdlFilter.pattern = ownedStrings[ownedStrings.size() - 1].c_str();
                mFilters.push_back(sdlFilter);
            }
        }

        virtual std::span<const SDL_DialogFileFilter> GetSDLDialogFileFilters() const override {
            return mFilters;
        }

    private:
        std::vector<SDL_DialogFileFilter> mFilters;
        Ref<RefCounted> mResourceHolder;
    };

    export Awaitable<std::vector<std::filesystem::path>> OpenFileDialogAsync(
        SDL_Window *parentWindow,
        Ref<IDialogFileFilterGroup> filterGroup) {
        struct UserDataPointerType {
            std::unique_ptr<std::promise<std::vector<std::filesystem::path>>> promise;
            Ref<IDialogFileFilterGroup> filterGroup;
        };

        std::unique_ptr<std::promise<std::vector<std::filesystem::path>>> promise = std::make_unique<std::promise<
            std::vector<std::filesystem::path>>>();

        auto future = promise->get_future();

        SDL_DialogFileCallback callback = [](void *userData, const char *const *filePaths, int filterIndex) {
            std::unique_ptr<UserDataPointerType> dataPtr =
                    std::unique_ptr<UserDataPointerType>(static_cast<UserDataPointerType *>(userData));
            std::vector<std::filesystem::path> paths;
            if (filePaths) {
                for (const char *const *ptr = filePaths; *ptr != nullptr; ++ptr) {
                    paths.emplace_back(std::filesystem::path(reinterpret_cast<const char8_t * const>(*ptr)));
                }
            }
            dataPtr->promise->set_value(std::move(paths));
        };

        auto filterPatterns = filterGroup->GetSDLDialogFileFilters();

        std::unique_ptr<UserDataPointerType> userDataPtr = std::make_unique<UserDataPointerType>();
        userDataPtr->promise = std::move(promise);
        userDataPtr->filterGroup = std::move(filterGroup);

        SDL_ShowOpenFileDialog(callback, userDataPtr.release(), parentWindow,
                               filterPatterns.data(), static_cast<int>(filterPatterns.size()), nullptr, true);

        co_return co_await std::move(future);
    }

    export Awaitable<std::vector<std::filesystem::path>> SelectDirectoryDialogAsync(SDL_Window *parentWindow) {
        std::unique_ptr<std::promise<std::vector<std::filesystem::path>>> promise = std::make_unique<std::promise<
            std::vector<
                std::filesystem::path>>>();

        auto future = promise->get_future();

        SDL_DialogFileCallback callback = [](void *userData, const char *const *filePaths, int filterIndex) {
            (void) filterIndex;
            auto promise = std::unique_ptr<std::promise<std::vector<std::filesystem::path>>>
                    (static_cast<std::promise<std::vector<std::filesystem::path>> *>(userData));
            std::vector<std::filesystem::path> paths;
            if (filePaths) {
                for (const char *const*ptr = filePaths; *ptr != nullptr; ++ptr) {
                    paths.emplace_back(std::filesystem::path(reinterpret_cast<const char8_t * const>(*ptr)));
                }
            }
            promise->set_value(std::move(paths));
        };

        SDL_ShowOpenFolderDialog(callback, promise.release(), parentWindow, nullptr, true);

        co_return co_await std::move(future);
    }

    export Awaitable<std::optional<std::filesystem::path>> SaveFileDialogAsync(
        SDL_Window *window,
        Ref<IDialogFileFilterGroup> filterGroup
    ) {
        struct UserDataPointerType {
            std::unique_ptr<std::promise<std::optional<std::filesystem::path>>> promise;
            Ref<IDialogFileFilterGroup> filterGroup;
        };

        auto promise = std::make_unique<std::promise<
            std::optional<std::filesystem::path>>>();

        auto future = promise->get_future();
        SDL_DialogFileCallback callback = [](void *userData, const char *const *filePaths, int filterIndex) {
            std::unique_ptr<UserDataPointerType> dataPtr =
                    std::unique_ptr<UserDataPointerType>(static_cast<UserDataPointerType *>(userData));
            if (filePaths && filePaths[0]) {
                dataPtr->promise->set_value(std::filesystem::path(reinterpret_cast<const char8_t *>(filePaths[0])));
            } else {
                dataPtr->promise->set_value(std::nullopt);
            }
        };

        std::unique_ptr<UserDataPointerType> userDataPtr = std::make_unique<UserDataPointerType>();
        userDataPtr->promise = std::move(promise);

        auto span = filterGroup->GetSDLDialogFileFilters();
        userDataPtr->filterGroup = std::move(filterGroup);

        SDL_ShowSaveFileDialog(callback, userDataPtr.release(), window,
                               span.data(), static_cast<int>(span.size()), nullptr);

        co_return co_await std::move(future);
    }
}
