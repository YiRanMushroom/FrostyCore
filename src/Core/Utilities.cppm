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

    template<typename T>
    using RefCntPtr = isptr::intrusive_shared_ptr<T, isptr::ref_counted_traits>;

    template<typename T>
    using WeakCntPtr = isptr::ref_counted<T, isptr::ref_counted_flags::provide_weak_references>::weak_ptr;

    export template<typename T>
    class RefCounted : private isptr::ref_counted<T, isptr::ref_counted_flags::provide_weak_references> {
        friend isptr::ref_counted<T, isptr::ref_counted_flags::provide_weak_references>;
        friend isptr::weak_reference<T>;
        friend isptr::ref_counted_traits;
        friend T;

        friend Ref<T>;
        friend Weak<T>;

    private:
        RefCounted() = default;

    public:
        Ref<T> RefFromThis() {
            return Ref<T>(RefCntPtr<T>::ref(static_cast<T *>(this)));
        }

        Weak<T> WeakFromThis() {
            return Weak<T>(RefFromThis());
        }

    protected:
        ~RefCounted() noexcept = default;
    };

    template<typename T, typename U>
    concept IsImplicitlyConvertibleTo = requires(T *t, U *u) {
        u = t;
    };

    template<typename T, typename U>
    concept IsExplicitlyConvertibleTo = requires(T *t) {
        static_cast<U *>(t);
    };

    template<typename T>
    class Ref {
    public:
        Ref() noexcept = default;

        Ref(nullptr_t) noexcept : mPtr(nullptr) {}

        friend class RefCounted<T>;
        friend class Weak<T>;

        explicit Ref(RefCntPtr<T> ptr) : mPtr(std::move(ptr)) {}

    public:
        template<typename... Args>
        static Ref Create(Args &&... args) {
            return Ref(RefCntPtr<T>::noref(new T(std::forward<Args>(args)...)));
        }

        T *Get() const noexcept { return mPtr.get(); }
        T &operator*() const noexcept { return *mPtr; }
        T *operator->() const noexcept { return mPtr.get(); }

        void Reset() noexcept { mPtr.reset(); }

        explicit operator bool() const noexcept { return static_cast<bool>(mPtr); }

        template<typename U> requires IsImplicitlyConvertibleTo<T, U>
        operator Ref<U>() const noexcept {
            return Ref<U>(RefCntPtr<U>::ref(static_cast<U *>(mPtr.get())));
        }

        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        explicit operator Ref<U>() const noexcept {
            return Ref<U>(RefCntPtr<U>::ref(static_cast<U *>(mPtr.get())));
        }

        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        Ref<U> As() const noexcept {
            return Ref<U>(RefCntPtr<U>::ref(static_cast<U *>(mPtr.get())));
        }

        auto operator<=>(const Ref &other) const noexcept = default;

        auto operator<=>(nullptr_t) const noexcept {
            if (mPtr) {
                return std::strong_ordering::greater;
            }
            return std::strong_ordering::equal;
        }

    private:
        RefCntPtr<T> mPtr{};
    };

    template<typename T>
    class Weak {
    public:
        Weak() noexcept = default;

        Weak(const Ref<T> &ref) : mWeakPtr(ref->get_weak_ptr()) {}

        Weak(WeakCntPtr<T> weakPtr) : mWeakPtr(std::move(weakPtr)) {}

        Ref<T> Lock() const noexcept {
            return Ref<T>(mWeakPtr->lock());
        }

    private:
        WeakCntPtr<T> mWeakPtr{};
    };

    export template<typename T, typename... Args>
    Ref<T> MakeRef(Args &&... args) {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }

    export template<typename I>
    class RefInterface : public I, public RefCounted<RefInterface<I>> {};
}
