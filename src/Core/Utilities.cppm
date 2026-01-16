export module Core.Utilities;

import Core.Prelude;
import <cassert>;
// import <intrusive_shared_ptr/ref_counted.h>;
// import <intrusive_shared_ptr/refcnt_ptr.h>;

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

    export class RefCounted;

    export template<typename>
    class Ref;

    export template<typename>
    class Weak;

    // Hack, definitely UB, should work though
    class RefCounted {
    protected:
        virtual ~RefCounted() noexcept = default;

    private:
        void FinalDeallocate() const {
            if (this->InitialAllocationPointer) {
                std::free(this->InitialAllocationPointer);
                sTotalAllocations.fetch_sub(1, std::memory_order_relaxed);
            } else {
                throw Engine::RuntimeException(
                    "Not using Make or Create to manage the life time of a RefCounted Object is prohibited,"
                    "Because it would corrupted the memory layout"
                );
            }
        }

    protected:
        template<typename>
        friend class Ref;

        template<typename>
        friend class Weak;

        void AddRefStrong() const noexcept {
            mStrongCount.fetch_add(1, std::memory_order_relaxed);
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void AddRefWeak() const noexcept {
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void SubRefStrong() const noexcept {
            // never delete this when strong count reaches zero, we should always this->~RefCounted() to destroy the
            // object, but we still need weak count to reach zero to free the memory
            // we then should use free directly because the destructor has already been called
            if (mStrongCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                this->~RefCounted();
                SubRefWeak();
            } else {
                SubRefWeak();
            }
        }

        void SubRefWeak() const noexcept {
            if (mWeakCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                FinalDeallocate();
                // std::free((void *) this);
                // std::cout << "Freeing RefCounted at " << this << std::endl;
            }
        }

    public:
        template<typename T = void, typename U>
        auto RefFromThis(this U &&self) -> Ref<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            return Ref<TargetType>((TargetType *) &self, ShareOwnership{});
        }

        template<typename T = void, typename U>
        auto WeakFromThis(
            this U &&self) -> Weak<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            return Weak<TargetType>((TargetType *) &self);
        }

    private:
        static_assert(std::is_trivially_destructible_v<std::atomic_size_t>,
                      "Atomic size type must be trivially destructible to make this work");

        mutable std::atomic_size_t mStrongCount{1};
        mutable std::atomic_size_t mWeakCount{1};

        void *InitialAllocationPointer = nullptr;

    public:
        struct TransferOwnership {};

        struct ShareOwnership {};

    public:
        static inline std::atomic_size_t sTotalAllocations{0};

        RefCounted() : mStrongCount(1), mWeakCount(1) {
            sTotalAllocations.fetch_add(1, std::memory_order_relaxed);
        }
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

        Ref(T *ptr, RefCounted::ShareOwnership) : mPtr(ptr) {
            if (mPtr) {
                mPtr->AddRefStrong();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

    private:
        Ref(T *ptr, RefCounted::TransferOwnership) : mPtr(ptr) {}

    public:
        Ref(const Ref &other) : mPtr(other.mPtr) {
            if (mPtr) {
                mPtr->AddRefStrong();
            }
        }

        Ref(Ref &&other) noexcept : mPtr(other.mPtr) {
            other.mPtr = nullptr;
        }

        Ref &operator=(const Ref &other) {
            if (this != &other) {
                if (mPtr) {
                    mPtr->SubRefStrong();
                }
                mPtr = other.mPtr;
                if (mPtr) {
                    mPtr->AddRefStrong();
                }
            }
            return *this;
        }

        Ref &operator=(Ref &&other) noexcept {
            if (this != &other) {
                if (mPtr) {
                    mPtr->SubRefStrong();
                }
                mPtr = other.mPtr;
                other.mPtr = nullptr;
            }
            return *this;
        }

    public:
        T *Get() const noexcept { return mPtr; }
        T &operator*() const noexcept { return *mPtr; }
        T *operator->() const noexcept { return mPtr; }

        void Reset() noexcept {
            if (mPtr) {
                mPtr->SubRefStrong();
                mPtr = nullptr;
            }
        }

        explicit operator bool() const noexcept { return mPtr != nullptr; }

        // bool operator==(std::nullptr_t) const noexcept {
        //     return mPtr == nullptr;
        // }

        auto operator<=>(const Ref &) const = default;

    public:
        ~Ref() noexcept {
            if (mPtr) {
                mPtr->SubRefStrong();
            }
        }

    public:
        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        Ref<U> As() const noexcept {
            return Ref<U>(static_cast<U *>(mPtr), RefCounted::ShareOwnership{});
        }

        template<typename U> requires IsImplicitlyConvertibleTo<T, U>
        operator Ref<U>() const noexcept {
            return Ref<U>(static_cast<U *>(mPtr), RefCounted::ShareOwnership{});
        }

        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        explicit operator Ref<U>() const noexcept {
            return Ref<U>(static_cast<U *>(mPtr), RefCounted::ShareOwnership{});
        }

        template<typename... Args>
        static Ref<T> Create(Args &&... args) {
            auto *allocated = new T(std::forward<Args>(args)...);
            allocated->InitialAllocationPointer = allocated;
            return Ref<T>(allocated, RefCounted::TransferOwnership{});
        }

    private:
        T *mPtr{nullptr};
    };

    template<typename T>
    class Weak {
    public:
        Weak() noexcept = default;

        Weak(std::nullptr_t) noexcept : mPtr(nullptr) {}

        Weak(const Ref<T> &ref) : mPtr(ref.mPtr) {
            if (mPtr) {
                mPtr->AddRefWeak();
            }
        }

        Weak(T *ptr) : mPtr(ptr) {
            if (mPtr) {
                mPtr->AddRefWeak();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

        Weak(const Weak &other) : mPtr(other.mPtr) {
            if (mPtr) {
                mPtr->AddRefWeak();
            }
        }

        Weak(Weak &&other) noexcept : mPtr(other.mPtr) {
            other.mPtr = nullptr;
        }

        Weak &operator=(const Weak &other) {
            if (this != &other) {
                if (mPtr) {
                    mPtr->SubRefWeak();
                }
                mPtr = other.mPtr;
                if (mPtr) {
                    mPtr->AddRefWeak();
                }
            }
            return *this;
        }

        Weak &operator=(Weak &&other) noexcept {
            if (this != &other) {
                if (mPtr) {
                    mPtr->SubRefWeak();
                }
                mPtr = other.mPtr;
                other.mPtr = nullptr;
            }
            return *this;
        }

    public:
        Ref<T> Lock() const noexcept {
            if (!mPtr) return nullptr;

            size_t current = mPtr->mStrongCount.load(std::memory_order_relaxed);
            while (current != 0) {
                if (mPtr->mStrongCount.compare_exchange_weak(current, current + 1,
                                                             std::memory_order_acquire,
                                                             std::memory_order_relaxed)) {
                    mPtr->mWeakCount.fetch_add(1, std::memory_order_relaxed);

                    return Ref<T>(mPtr, RefCounted::TransferOwnership{});
                }
            }
            return nullptr;
        }

    public:
        ~Weak() noexcept {
            if (mPtr) {
                mPtr->SubRefWeak();
            }
        }

    private:
        T *mPtr{nullptr};
    };

    export template<typename T>
    class RefInterface : public RefCounted, public T {};

    export template<typename T, typename... Args>
    Ref<T> MakeRef(Args &&... args) {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }
}
