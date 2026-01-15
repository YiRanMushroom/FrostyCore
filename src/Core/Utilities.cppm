export module Core.Utilities;

import Core.Prelude;
import <intrusive_shared_ptr/ref_counted.h>;
import <intrusive_shared_ptr/refcnt_ptr.h>;

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
        std::shared_ptr<std::atomic<bool>> mInitialized = std::make_unique<std::atomic<bool>>(false);
    };

    export template<typename T>
    class Ref;

    export template<typename T>
    class Weak;

    export template<typename T>
    class RefCounted : public isptr::ref_counted<T, isptr::ref_counted_flags::provide_weak_references> {
        friend isptr::ref_counted<T, isptr::ref_counted_flags::provide_weak_references>;

    public:
        Ref<T> RefFromThis() {
            return Ref<T>(isptr::refcnt_retain(static_cast<T *>(this)));
        }

        Weak<T> WeakFromThis() {
            return Weak<T>(RefFromThis());
        }

    protected:
        ~RefCounted() noexcept = default;
    };

    template<typename T>
    class Ref {
    public:
        Ref() noexcept = default;

    private:
        friend class RefCounted<T>;
        explicit Ref(isptr::refcnt_ptr<T> ptr) : mPtr(std::move(ptr)) {}
    public:

        template<typename... Args>
        static Ref Create(Args &&... args) {
            return Ref(isptr::make_refcnt<T>(std::forward<Args>(args)...));
        }

        T *Get() const noexcept { return mPtr.get(); }
        T &operator*() const noexcept { return *mPtr; }
        T *operator->() const noexcept { return mPtr.get(); }

        void Reset() noexcept { mPtr.reset(); }

        explicit operator bool() const noexcept { return static_cast<bool>(mPtr); }

    private:
        friend class Weak<T>;
        isptr::refcnt_ptr<T> mPtr{nullptr};
    };

    template<typename T>
    class Weak {
    public:
        Weak() noexcept = default;

        Weak(const Ref<T> &ref) : mWeakPtr(ref.mPtr) {}

        Ref<T> Lock() const noexcept {
            return Ref<T>(mWeakPtr.lock());
        }

    private:
        isptr::refcnt_ptr<T>::weak mWeakPtr;
    };

    export template<typename T, typename... Args>
    Ref<T> MakeRef(Args &&... args) {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }
}
