export module Core.Utilities:Initializer;

import Core.Prelude;

import :Memory;

import Core.Coroutine;

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

        AsyncInitializationContext(IRefCounted *owner) : mOwner(owner->RefFromThis()) {}

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
                           [owner = mOwner, memberPtr,
                               func = std::move(initFunc)] mutable {
                               ResultType result = std::invoke(std::move(func));
                               return std::move_only_function<void()>(
                                   [owner = std::move(owner), memberPtr, result = std::move(result)]() mutable {
                                       if (auto self = owner.Lock()) {
                                           auto derivedSelf = static_cast<DerivedType *>(self.Get());
                                           derivedSelf->*memberPtr = std::move(result);
                                       }
                                   });
                           }));
        }

        void Destroy() {
            for (auto &future: mInitFutures) {
                auto pullFunc = future.get();
                std::invoke(std::move(pullFunc));
            }
            mInitFutures.clear();
        }

        ~AsyncInitializationContext() {
            Destroy();
            // if (!m_IsDestroyed) {
            //     std::cerr << "Error: AsyncInitializationContext must be explicitly destroyed before destruction."
            //                " Or else the result is fatal, rather than simple resource leak." << std::endl;
            //     throw RuntimeException("AsyncInitializationContext must be explicitly destroyed before destruction.");
            // }
        }

    private:
        bool m_IsDestroyed{false};
        Weak<IRefCounted> mOwner;
        std::vector<std::future<std::move_only_function<void()>>> mInitFutures;
    };
}
