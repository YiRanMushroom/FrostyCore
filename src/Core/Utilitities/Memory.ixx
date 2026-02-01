export module Core.Utilities:Memory;

import Core.Prelude;
import Core.Exception;

import :TypeTraits;
import :MultiInterface;

namespace
Engine {
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
        template<typename T = void, typename U> requires std::is_base_of_v<RefCounted, std::remove_cvref_t<U>>
        auto RefFromThis(this U &&self) -> Ref<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            Ref<TargetType> result;
            result.mPtr = (TargetType *) &self;
            result.mCounterAddress = static_cast<RefCounted *>(&self);
            if (result.mCounterAddress) {
                result.mCounterAddress->AddRefStrong();
            }
            return result;
        }

        template<typename T = void, typename U> requires std::is_base_of_v<RefCounted, std::remove_cvref_t<U>>
        auto WeakFromThis(
            this U &&self) -> Weak<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            Weak<TargetType> result;
            result.mPtr = (TargetType *) &self;
            result.mCounterAddress = static_cast<RefCounted *>(&self);
            if (result.mCounterAddress) {
                result.mCounterAddress->AddRefWeak();
            }
            return result;
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

    export template<typename T>
    class Borrowed;

    template<typename T>
    class Ref {
    public:
        Ref() noexcept = default;

        Ref(nullptr_t) noexcept : mPtr(nullptr) {}

        Ref(T *ptr, RefCounted::ShareOwnership) requires std::is_base_of_v<RefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<RefCounted *>(ptr)) {
            if (mCounterAddress) {
                mCounterAddress->AddRefStrong();
            }
        }

        Ref(T *ptr, RefCounted *counterAddress, RefCounted::ShareOwnership)
            : mPtr(ptr), mCounterAddress(counterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefStrong();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

        friend RefCounted;

    private:
        Ref(T *ptr, RefCounted::TransferOwnership) requires std::is_base_of_v<RefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<RefCounted *>(ptr)) {}

    public:
        Ref(const Ref &other) : mPtr(other.mPtr), mCounterAddress(other.mCounterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefStrong();
            }
        }

        Ref(Ref &&other) noexcept : mPtr(other.mPtr), mCounterAddress(other.mCounterAddress) {
            other.mPtr = nullptr;
            other.mCounterAddress = nullptr;
        }

        Ref &operator=(const Ref &other) {
            if (this != &other) {
                if (mCounterAddress) {
                    mCounterAddress->SubRefStrong();
                }
                mPtr = other.mPtr;
                mCounterAddress = other.mCounterAddress;
                if (mCounterAddress) {
                    mCounterAddress->AddRefStrong();
                }
            }
            return *this;
        }

        Ref &operator=(Ref &&other) noexcept {
            if (this != &other) {
                if (mCounterAddress) {
                    mCounterAddress->SubRefStrong();
                }
                mPtr = other.mPtr;
                other.mPtr = nullptr;

                mCounterAddress = other.mCounterAddress;
                other.mCounterAddress = nullptr;
            }
            return *this;
        }

    public:
        T *Get() const noexcept { return mPtr; }
        T &operator*() const noexcept { return *mPtr; }
        T *operator->() const noexcept { return mPtr; }

        void Reset() noexcept {
            if (mCounterAddress) {
                mCounterAddress->SubRefStrong();

                mCounterAddress = nullptr;
                mPtr = nullptr;
            }
        }

        explicit operator bool() const noexcept { return mPtr != nullptr; }

        auto operator<=>(const Ref &other) const = default;

        [[nodiscard]] Weak<T> Weak() const noexcept;

        [[nodiscard]] Borrowed<T> Borrow() const noexcept;

    public:
        ~Ref() noexcept {
            if (mCounterAddress) {
                mCounterAddress->SubRefStrong();
            }
        }

    public:
        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        Ref<U> As() const noexcept {
            Ref<U> result;
            result.mPtr = static_cast<U *>(mPtr);
            result.mCounterAddress = mCounterAddress;
            if (result.mCounterAddress) {
                result.mCounterAddress->AddRefStrong();
            }
            return result;
        }

        template<typename U> requires IsImplicitlyConvertibleTo<T, U>
        operator Ref<U>() const noexcept {
            return As<U>();
        }

        template<typename U> requires IsExplicitlyConvertibleTo<T, U>
        explicit operator Ref<U>() const noexcept {
            return As<U>();
        }

        template<typename... Args> requires std::is_base_of_v<RefCounted, T>
        // enable this only for RefCounted derived types
        static Ref<T> Create(Args &&... args) {
            void *rawMemory = std::malloc(sizeof(T));
            if (!rawMemory) throw std::bad_alloc();

            T *instance = nullptr;
            try {
                instance = new(rawMemory) T(std::forward<Args>(args)...);
            } catch (...) {
                std::free(rawMemory);
                throw;
            }

            instance->InitialAllocationPointer = rawMemory;

            return Ref<T>(instance, RefCounted::TransferOwnership{});
        }

    private:
        T *mPtr{nullptr};
        RefCounted *mCounterAddress{nullptr};
    };

    template<typename T>
    class Weak {
    public:
        Weak() noexcept = default;

        Weak(std::nullptr_t) noexcept : mPtr(nullptr) {}

        Weak(const Ref<T> &ref) : mPtr(ref.mPtr), mCounterAddress(ref.mCounterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        Weak(T *ptr, RefCounted::ShareOwnership) requires std::is_base_of_v<RefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<RefCounted *>(ptr)) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        Weak(T *ptr, RefCounted *counterAddress, RefCounted::ShareOwnership)
            : mPtr(ptr), mCounterAddress(counterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

        friend RefCounted;

        Weak(const Weak &other) : mPtr(other.mPtr), mCounterAddress(other.mCounterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        Weak(Weak &&other) noexcept : mPtr(other.mPtr), mCounterAddress(other.mCounterAddress) {
            other.mPtr = nullptr;
            other.mCounterAddress = nullptr;
        }

        Weak &operator=(const Weak &other) {
            if (this != &other) {
                if (mCounterAddress) {
                    mCounterAddress->SubRefWeak();
                }

                mPtr = other.mPtr;
                mCounterAddress = other.mCounterAddress;

                if (mCounterAddress) {
                    mCounterAddress->AddRefWeak();
                }
            }
            return *this;
        }

        Weak &operator=(Weak &&other) noexcept {
            if (this != &other) {
                if (mCounterAddress) {
                    mCounterAddress->SubRefWeak();
                }
                mPtr = other.mPtr;
                mCounterAddress = other.mCounterAddress;

                other.mPtr = nullptr;
                other.mCounterAddress = nullptr;
            }
            return *this;
        }

    public:
        [[nodiscard]] Ref<T> Lock() const noexcept;

        [[nodiscard]] Borrowed<T> Borrow() const noexcept;

    public:
        ~Weak() noexcept {
            if (mCounterAddress) {
                mCounterAddress->SubRefWeak();
            }
        }

    private:
        T *mPtr{nullptr};
        RefCounted *mCounterAddress{nullptr};
    };

    template<typename T>
    Weak<T> Ref<T>::Weak() const noexcept {
        return Engine::Weak<T>(*this);
    }

    template<typename T>
    Borrowed<T> Ref<T>::Borrow() const noexcept {
        return MultiInterface<T, RefCounted>::CreateFromRawPointersUnsafe(
            mPtr,
            mCounterAddress
        );
    }

    template<typename T>
    Ref<T> Weak<T>::Lock() const noexcept {
        if (!mCounterAddress) return nullptr;

        size_t current = mCounterAddress->mStrongCount.load(std::memory_order_relaxed);
        while (current != 0) {
            if (mCounterAddress->mStrongCount.compare_exchange_weak(current, current + 1,
                                                                    std::memory_order_acquire,
                                                                    std::memory_order_relaxed)) {
                mCounterAddress->mWeakCount.fetch_add(1, std::memory_order_relaxed);

                Ref<T> result;
                result.mPtr = mPtr;
                result.mCounterAddress = mCounterAddress;
                return result;
            }
        }
        return nullptr;
    }

    template<typename T>
    Borrowed<T> Weak<T>::Borrow() const noexcept {
        return MultiInterface<T, RefCounted>::CreateFromRawPointersUnsafe(
            mPtr,
            mCounterAddress
        );
    }

    export template<typename T>
    class RefInterface : public RefCounted, public T {
    public:
        template<typename... Args>
        RefInterface(Args &&... args) : T(std::forward<Args>(args)...) {} // forward constructor args
    };

    export template<typename T, typename... Args> requires std::is_base_of_v<RefCounted, T>
    Ref<T> MakeRef(Args &&... args) {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }

    export template<typename T, typename... Args> requires !std::is_base_of_v<RefCounted, T>
    Ref<T> MakeRef(Args &&... args) {
        return Ref<RefInterface<T>>::Create(std::forward<Args>(args)...).template As<T>();
    }

    export template<typename T>
    class Borrowed {
    public:
        friend MultiInterface<T, RefCounted>;

        template<typename>
        friend class Borrowed;

    private:
        MultiInterface<T, RefCounted> mBase;

    public:
        operator const MultiInterface<T, RefCounted> &() const {
            return mBase;
        }

        operator MultiInterface<T, RefCounted> &() {
            return mBase;
        }

        Borrowed(const MultiInterface<T, RefCounted> &base) : mBase(base) {}

        Borrowed() = default;

        template<typename U> requires (IsImplicitlyConvertibleTo<U, T>)
        Borrowed(const Borrowed<U> &other)
            : mBase(other.mBase.template Into<MultiInterface<T, RefCounted>>()) {}

    public:
        bool HasValue() const {
            return mBase.HasValue();
        }

        operator bool() const {
            return mBase;
        }

        template<typename U> requires (IsImplicitlyConvertibleTo<T, U> || IsImplicitlyConvertibleTo<RefCounted, U>)
        U *GetInterface() const {
            return mBase.template GetInterface<U>();
        }

        Ref<T> Ref() const {
            return Engine::Ref<T>(this->GetInterface<T>(), this->GetInterface<RefCounted>(),
                                  RefCounted::ShareOwnership{});
        }

        Weak<T> Weak() const {
            return Engine::Weak<T>(this->GetInterface<T>(), this->GetInterface<RefCounted>(),
                                   RefCounted::ShareOwnership{});
        }

        template<typename U> requires (IsImplicitlyConvertibleTo<T, U> || IsImplicitlyConvertibleTo<RefCounted, U>)
        Borrowed<U> Into() const {
            U *ptr = static_cast<U *>(this->GetInterface<T>());
            RefCounted *counter = this->GetInterface<RefCounted>();
            return MultiInterface<U, RefCounted>::CreateFromRawPointersUnsafe(ptr, counter);
        }

        template<typename U> requires (IsExplicitlyConvertibleTo<T, U> || IsExplicitlyConvertibleTo<RefCounted, U>)
        Borrowed<U> As() const noexcept {
            U *ptr = static_cast<U *>(this->GetInterface<T>());
            RefCounted *counter = this->GetInterface<RefCounted>();
            return MultiInterface<U, RefCounted>::CreateFromRawPointersUnsafe(ptr, counter);
        }
    };
}
