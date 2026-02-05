export module Core.Utilities:Initializer;

import Core.Prelude;

import :Memory;

namespace
Engine {
    export class Initializer {
    public:
        Initializer() = default;

        template<typename FuncType> requires std::is_invocable_r_v<void, FuncType>
        Initializer(FuncType &&initFunc) {
            mInitialized = std::make_shared<std::atomic<bool>>(false);
            mInitFuture = std::async(std::launch::async,
                                     [func = std::forward<FuncType>(initFunc), Initialized = mInitialized]() {
                                         func();
                                         Initialized->store(true, std::memory_order_release);
                                     });
        }

        Initializer(Initializer &&other) = default;

        Initializer &operator=(Initializer &&other) = default;

        bool IsInitialized() {
            if (mInitialized->load(std::memory_order_acquire)) {
                return true;
            }

            if (mInitFuture.valid()) {
                auto status = mInitFuture.wait_for(std::chrono::seconds(0));
                if (status == std::future_status::ready) {
                    mInitFuture.get(); // to propagate exceptions
                    return true;
                }
            }

            return false;
        }

        explicit operator bool() {
            return IsInitialized();
        }

        std::future<void> Wait() {
            return std::async([this]() {
                if (mInitFuture.valid()) {
                    mInitFuture.get();
                }
            });
        }

        void Reset() {
            Wait().get();
            mInitialized = {};
        }

    private:
        std::future<void> mInitFuture;
        std::shared_ptr<std::atomic<bool>> mInitialized = std::make_shared<std::atomic<bool>>(false);
    };

    export class AsyncInitializationContext {
    public:
        AsyncInitializationContext() = delete;

        AsyncInitializationContext(IRefCounted *owner) : mOwner(owner) {}

        void Pull() {
            std::erase_if(mInitFutures, [](auto &future) {
                auto status = future.wait_for(std::chrono::seconds(0));
                if (status == std::future_status::ready) {
                    auto pullFunc = future.get();
                    std::invoke(std::move(pullFunc));
                    return true;
                }
                return false;
            });
        }

        template<typename DerivedType, typename ResultType>
        void EnqueueInitialization(ResultType DerivedType::*memberPtr,
                                   std::move_only_function<std::type_identity_t<ResultType>()> initFunc) {
            mInitFutures.push_back(
                std::async(std::launch::async,
                           [owner = mOwner->RefFromThis<DerivedType>(), memberPtr,
                               func = std::move(initFunc)] mutable {
                               ResultType result = std::invoke(std::move(func));
                               return std::move_only_function<void()>(
                                   [owner = std::move(owner), memberPtr, result = std::move(result)]() mutable {
                                       owner->*memberPtr = std::move(result);
                                   });
                           }));
        }

        void Destroy() {
            for (auto &future: mInitFutures) {
                if (future.valid()) {
                    future.get();
                }
            }
        }

        ~AsyncInitializationContext() {
            Destroy();
        }

    private:
        IRefCounted *mOwner;
        std::vector<std::future<std::move_only_function<void()>>> mInitFutures;
    };
}
