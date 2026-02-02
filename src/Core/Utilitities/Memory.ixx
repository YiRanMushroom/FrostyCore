export module Core.Utilities:Memory;

import Core.Prelude;
import Core.Exception;

import :TypeTraits;
import :MultiInterface;
import :Tags;

namespace
Engine {
    export class RefCounted;

    export template<typename>
    class Ref;

    export template<typename>
    class Weak;

    export class IRefCounted;

    export struct RefCountedVTable {
        void (IRefCounted::*AddRefStrong)() const noexcept;
        void (IRefCounted::*AddRefWeak)() const noexcept;
        void (IRefCounted::*SubRefStrong)() const noexcept;
        void (IRefCounted::*SubRefWeak)() const noexcept;
        bool (IRefCounted::*TryAddRefStrong)() const noexcept;
    };

    class IRefCounted {
    protected:
        virtual ~IRefCounted() noexcept = default;

    public:
        virtual void AddRefStrong() const noexcept = 0;

        virtual void AddRefWeak() const noexcept = 0;

        virtual void SubRefStrong() const noexcept = 0;

        virtual void SubRefWeak() const noexcept = 0;

        // Try to upgrade from weak to strong reference, returns true if successful
        // This is used by Weak::Lock() to atomically check and increment the strong count
        [[nodiscard]] virtual bool TryAddRefStrong() const noexcept = 0;

    public:
        template<typename T = void, typename U> requires std::is_base_of_v<IRefCounted, std::remove_cvref_t<U>>
        auto RefFromThis(this U &&self) -> Ref<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            Ref<TargetType> result;
            result.mPtr = static_cast<TargetType *>(&self);
            result.mCounterAddress = static_cast<IRefCounted *>(&self);
            if (result.mCounterAddress) {
                result.mCounterAddress->AddRefStrong();
            }
            return result;
        }

        template<typename T = void, typename U> requires std::is_base_of_v<IRefCounted, std::remove_cvref_t<U>>
        auto WeakFromThis(
            this U &&self) -> Weak<std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>> {
            using TargetType = std::conditional_t<std::is_same_v<void, T>, std::remove_cvref_t<U>, T>;
            Weak<TargetType> result;
            result.mPtr = static_cast<TargetType *>(&self);
            result.mCounterAddress = static_cast<IRefCounted *>(&self);
            if (result.mCounterAddress) {
                result.mCounterAddress->AddRefWeak();
            }
            return result;
        }
    };

    class RefCounted : public IRefCounted {
        friend class IRefCounted;

    protected:
        ~RefCounted() noexcept override = default;

    private:
        void FinalDeallocate() const {
            std::free(const_cast<void *>(mMallocPointer));
            sTotalAllocations.fetch_sub(1, std::memory_order_relaxed);
        }

    protected:
        template<typename>
        friend class Ref;

        template<typename>
        friend class Weak;

        void AddRefStrong() const noexcept override {
            mStrongCount.fetch_add(1, std::memory_order_relaxed);
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void AddRefWeak() const noexcept override {
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void SubRefStrong() const noexcept override {
            // never delete this when strong count reaches zero, we should always this->~RefCounted() to destroy the
            // object, but we still need weak count to reach zero to free the memory
            // we then should use free directly because the destructor has already been called
            if (mStrongCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                mMallocPointer = dynamic_cast<const void *>(this);
                this->~RefCounted();
                new(static_cast<void *>(const_cast<RefCounted *>(this)))
                        RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction{});
                SubRefWeak();
            } else {
                SubRefWeak();
            }
        }

        void SubRefWeak() const noexcept override {
            if (mWeakCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                FinalDeallocate();
            }
        }

        bool TryAddRefStrong() const noexcept override {
            size_t current = mStrongCount.load(std::memory_order_relaxed);
            while (current != 0) {
                if (mStrongCount.compare_exchange_weak(current, current + 1,
                                                       std::memory_order_acquire,
                                                       std::memory_order_relaxed)) {
                    mWeakCount.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
            return false;
        }

    private:
        static_assert(std::is_trivially_destructible_v<std::atomic_size_t>,
                      "Atomic size type must be trivially destructible to make this work");

        union {
            struct {
                // strong count includes weak references
                // when strong count reaches zero, the object is destroyed
                // when weak count reaches zero, the memory is freed
                mutable std::atomic_size_t mStrongCount;
                mutable std::atomic_size_t mWeakCount;
            };

            alignas(16) char mPadding[sizeof(std::atomic_size_t) * 2];
        };

        mutable const void *mMallocPointer;

    private:
        struct ConstructionFlag {
            struct BeginLifetime {};

            struct RestartRefCountedLifetimeAfterDerivedDestruction {};
        };

        RefCounted(ConstructionFlag::BeginLifetime) : mStrongCount(1), mWeakCount(1), mMallocPointer(nullptr) {
            sTotalAllocations.fetch_add(1, std::memory_order_relaxed);
        }

        RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction) {
            // Do not initialize anything, correct data is already in place
        }

    public:
        static inline std::atomic_size_t sTotalAllocations{0};

        RefCounted() : RefCounted(ConstructionFlag::BeginLifetime{}) {}
    };

    export template<typename T>
    class Borrowed;

    template<typename T>
    class Ref {
    public:
        Ref() noexcept = default;

        Ref(nullptr_t) noexcept : mPtr(nullptr) {}

        Ref(T *ptr, ResourceOwnership::Tags::Shared) requires std::is_base_of_v<IRefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<IRefCounted *>(ptr)) {
            if (mCounterAddress) {
                mCounterAddress->AddRefStrong();
            }
        }

        Ref(T *ptr, IRefCounted *counterAddress, ResourceOwnership::Tags::Shared)
            : mPtr(ptr), mCounterAddress(counterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefStrong();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

        friend IRefCounted;
        friend RefCounted;

    private:
        Ref(T *ptr, ResourceOwnership::Tags::Transferred) requires std::is_base_of_v<IRefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<IRefCounted *>(ptr)) {}

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

            return Ref<T>(instance, ResourceOwnership::Tags::Transferred{});
        }

    private:
        T *mPtr{nullptr};
        IRefCounted *mCounterAddress{nullptr};
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

        Weak(T *ptr, ResourceOwnership::Tags::Shared) requires std::is_base_of_v<IRefCounted, T>
            : mPtr(ptr), mCounterAddress(static_cast<IRefCounted *>(ptr)) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        Weak(T *ptr, IRefCounted *counterAddress, ResourceOwnership::Tags::Shared)
            : mPtr(ptr), mCounterAddress(counterAddress) {
            if (mCounterAddress) {
                mCounterAddress->AddRefWeak();
            }
        }

        template<typename U>
        friend class Ref;

        template<typename U>
        friend class Weak;

        friend IRefCounted;
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
        IRefCounted *mCounterAddress{nullptr};
    };

    template<typename T>
    Weak<T> Ref<T>::Weak() const noexcept {
        return Engine::Weak<T>(*this);
    }

    template<typename T>
    Borrowed<T> Ref<T>::Borrow() const noexcept {
        return MultiInterface<T, IRefCounted>::CreateFromRawPointersUnsafe(
            mPtr,
            mCounterAddress
        );
    }

    template<typename T>
    Ref<T> Weak<T>::Lock() const noexcept {
        if (!mCounterAddress) return nullptr;

        if (mCounterAddress->TryAddRefStrong()) {
            Ref<T> result;
            result.mPtr = mPtr;
            result.mCounterAddress = mCounterAddress;
            return result;
        }

        return nullptr;
    }

    template<typename T>
    Borrowed<T> Weak<T>::Borrow() const noexcept {
        return MultiInterface<T, IRefCounted>::CreateFromRawPointersUnsafe(
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
        friend MultiInterface<T, IRefCounted>;

        template<typename>
        friend class Borrowed;

    private:
        MultiInterface<T, IRefCounted> mBase;

    public:
        operator const MultiInterface<T, IRefCounted> &() const {
            return mBase;
        }

        operator MultiInterface<T, IRefCounted> &() {
            return mBase;
        }

        Borrowed(const MultiInterface<T, IRefCounted> &base) : mBase(base) {}

        Borrowed() = default;

        template<typename U> requires (IsImplicitlyConvertibleTo<U, T>)
        Borrowed(const Borrowed<U> &other)
            : mBase(other.mBase.template Into<MultiInterface<T, IRefCounted>>()) {}

    public:
        bool HasValue() const {
            return mBase.HasValue();
        }

        operator bool() const {
            return mBase;
        }

        template<typename U> requires (IsImplicitlyConvertibleTo<T, U> || IsImplicitlyConvertibleTo<IRefCounted, U>)
        U *GetInterface() const {
            return mBase.template GetInterface<U>();
        }

        Ref<T> Ref() const {
            return Engine::Ref<T>(this->GetInterface<T>(), this->GetInterface<IRefCounted>(),
                                  ResourceOwnership::Tags::Shared{});
        }

        Weak<T> Weak() const {
            return Engine::Weak<T>(this->GetInterface<T>(), this->GetInterface<IRefCounted>(),
                                   ResourceOwnership::Tags::Shared{});
        }

        template<typename U> requires (IsImplicitlyConvertibleTo<T, U> || IsImplicitlyConvertibleTo<IRefCounted, U>)
        Borrowed<U> Into() const {
            U *ptr = static_cast<U *>(this->GetInterface<T>());
            IRefCounted *counter = this->GetInterface<IRefCounted>();
            return MultiInterface<U, IRefCounted>::CreateFromRawPointersUnsafe(ptr, counter);
        }

        template<typename U> requires (IsExplicitlyConvertibleTo<T, U> || IsExplicitlyConvertibleTo<IRefCounted, U>)
        Borrowed<U> As() const noexcept {
            U *ptr = static_cast<U *>(this->GetInterface<T>());
            IRefCounted *counter = this->GetInterface<IRefCounted>();
            return MultiInterface<U, IRefCounted>::CreateFromRawPointersUnsafe(ptr, counter);
        }
    };
}
