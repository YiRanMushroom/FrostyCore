export module Core.Utilities:Memory;

import Core.Prelude;
import Core.Exception;

import :TypeTraits;
import :MultiInterface;
import :Tags;

namespace
Engine {
    export class IRefCounted;

    export class RefCounted;

    export template<typename>
    class Ref;

    export template<typename>
    class Weak;

    export struct RefCountedVTable {
        void (IRefCounted::*AddRefStrong)() const noexcept;

        void (IRefCounted::*AddRefWeak)() const noexcept;

        void (IRefCounted::*SubRefStrong)() const noexcept;

        void (IRefCounted::*SubRefWeak)() const noexcept;

        bool (IRefCounted::*TryAddRefStrong)() const noexcept;
    };

    class IRefCounted {
    public:
        virtual ~IRefCounted() noexcept = default;

    protected:
        const RefCountedVTable *mVTable;

    public:
        void AddRefStrong() const noexcept {
            const IRefCounted *self = std::launder(this);
            (self->*mVTable->AddRefStrong)();
        }

        void AddRefWeak() const noexcept {
            const IRefCounted *self = std::launder(this);
            (self->*mVTable->AddRefWeak)();
        }

        void SubRefStrong() const noexcept {
            const IRefCounted *self = std::launder(this);
            (self->*mVTable->SubRefStrong)();
        }

        void SubRefWeak() const noexcept {
            const IRefCounted *self = std::launder(this);
            (self->*mVTable->SubRefWeak)();
        }

        // Try to upgrade from weak to strong reference, returns true if successful
        // This is used by Weak::Lock() to atomically check and increment the strong count
        [[nodiscard]] bool TryAddRefStrong() const noexcept {
            const IRefCounted *self = std::launder(this);
            return (self->*mVTable->TryAddRefStrong)();
        }

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

    protected:
        IRefCounted(RefCountedVTable const *vtable) : mVTable(vtable) {}

        IRefCounted() {
            // Do not initialize anything, correct vtable pointer is already in place
        }
    };

    class RefCounted : public IRefCounted {
        friend class IRefCounted;

    public:
        ~RefCounted() noexcept override = default;

    private:
        void FinalDeallocate() const {
            sTotalAllocations.fetch_sub(1, std::memory_order_relaxed);
            std::free(const_cast<void *>(mMallocPointer));
        }

    protected:
        template<typename>
        friend class Ref;

        template<typename>
        friend class Weak;

    private:
        void AddRefStrongImpl() const noexcept {
            mStrongCount.fetch_add(1, std::memory_order_relaxed);
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void AddRefWeakImpl() const noexcept {
            mWeakCount.fetch_add(1, std::memory_order_relaxed);
        }

        void SubRefStrongImpl() const noexcept {
            // never delete this(actually I am deleting this but not freeing the memory)
            // when strong count reaches zero, we should always this->~RefCounted() to destroy the
            // object, but we still need weak count to reach zero to free the memory
            // we then should use free directly because the destructor has already been called
            if (mStrongCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                // mMallocPointer = dynamic_cast<const void *>(this);
                // this->~RefCounted();
                // new(static_cast<void *>(const_cast<RefCounted *>(this)))
                //         RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction{});
                TransitToDerivedDestructedState();
                SubRefWeakImpl();
            } else {
                SubRefWeakImpl();
            }
        }

        inline static thread_local void* sLastMallocPointer;

        void operator delete(void *ptr) noexcept {
            sLastMallocPointer = ptr; // Very hacky but whatever
        }

        void TransitToDerivedDestructedStateImpl1() const noexcept {
            delete this; // Not actually freeing the memory, I fucking hate this so much
            mMallocPointer = sLastMallocPointer; // Thread local so it is safe
            sLastMallocPointer = nullptr;
            new (static_cast<void *>(const_cast<RefCounted *>(this)))
                    RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction{}); // Fucking does nothing
        }

        // Alternative implementation that doesn't use operator delete hackery
        // I don't like it as much because it does an extra dynamic_cast, although we have RTTI enabled anyway
        // and dynamic cast to void* does not introduce any overhead other than vtable lookup
        // I fucking gave up not making it not hacky, so whatever
        // This whole file has hacks everywhere
        // void TransitToDerivedDestructedStateImpl2() const noexcept {
        //     mMallocPointer = dynamic_cast<const void *>(this);
        //     this->~RefCounted();
        //     new(static_cast<void *>(const_cast<RefCounted *>(this)))
        //             RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction{});
        // }

        void TransitToDerivedDestructedState() const noexcept {
            TransitToDerivedDestructedStateImpl1();
        }

        void SubRefWeakImpl() const noexcept {
            if (mWeakCount.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                FinalDeallocate();
            }
        }

        bool TryAddRefStrongImpl() const noexcept {
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
        static constexpr RefCountedVTable sVTable{
            .AddRefStrong = static_cast<void (IRefCounted::*)() const noexcept>(&RefCounted::AddRefStrongImpl),
            .AddRefWeak = static_cast<void (IRefCounted::*)() const noexcept>(&RefCounted::AddRefWeakImpl),
            .SubRefStrong = static_cast<void (IRefCounted::*)() const noexcept>(&RefCounted::SubRefStrongImpl),
            .SubRefWeak = static_cast<void (IRefCounted::*)() const noexcept>(&RefCounted::SubRefWeakImpl),
            .TryAddRefStrong = static_cast<bool (IRefCounted::*)() const noexcept>(&RefCounted::TryAddRefStrongImpl)
        };

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

        RefCounted(ConstructionFlag::BeginLifetime) : IRefCounted(&sVTable), mStrongCount(1), mWeakCount(1),
                                                      mMallocPointer(nullptr) {
        }

        RefCounted(ConstructionFlag::RestartRefCountedLifetimeAfterDerivedDestruction) : IRefCounted{} {
            // Do not initialize anything, correct data is already in place
            // mVTable pointer is trivially destructible and will remain valid
        }

    public:
        static inline std::atomic_size_t sTotalAllocations{0};

        RefCounted() : RefCounted(ConstructionFlag::BeginLifetime{}) {
        }
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

            RefCounted::sTotalAllocations.fetch_add(1, std::memory_order_relaxed);

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
