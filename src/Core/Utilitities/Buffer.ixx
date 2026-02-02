export module Core.Utilities:Buffer;

import Core.Prelude;
import :Tags;
import :Memory;

namespace
Engine {
    export template<typename T>
    class IBuffer {
    public:
        virtual ~IBuffer() = default;

        virtual T *Data() const = 0;

        virtual size_t Size() const = 0;

        size_t ByteSize() const {
            return Size() * sizeof(T);
        }

        std::span<T> AsSpan() const {
            return std::span<T>(Data(), Size());
        }
    };

    template<typename T>
    struct DefaultBufferDeleter {
        void operator()(T *ptr) const {
            delete[] ptr;
        }
    };

    export template<typename T, typename Deleter = DefaultBufferDeleter<T>>
    class DynamicBuffer : public IBuffer<T> {
    public:
        // virtual ~DynamicBuffer() override = default;

        DynamicBuffer(size_t size, ResourceState::Uninitialized)
            : mSize(size), mData(static_cast<T *>(new T[size])) {}

        DynamicBuffer(size_t size, ResourceState::DefaultInitialized)
            : mSize(size), mData(std::make_unique<T[]>(size)) {}

        DynamicBuffer(T *data, size_t size, ResourceOwnership::Borrowed)
            : mSize(size), mData(data) {}

        DynamicBuffer(T *data, size_t size, ResourceOwnership::Static)
            : mSize(size), mData(data) {}

        DynamicBuffer(T *data, size_t size, ResourceOwnership::Transferred)
            : mSize(size), mData(std::unique_ptr<T[], Deleter>(data)) {}

        DynamicBuffer(ResourceState::Uninitialized)
            : mSize(0), mData(static_cast<T *>(nullptr)) {}

        virtual T *Data() const override {
            if (std::holds_alternative<std::unique_ptr<T[], Deleter>>(mData)) {
                return std::get<std::unique_ptr<T[], Deleter>>(mData).get();
            } else {
                return std::get<T *>(mData);
            }
        }

        virtual size_t Size() const override {
            return mSize;
        }

        // DynamicBuffer(const DynamicBuffer &) = delete;
        // DynamicBuffer(DynamicBuffer &&) = default;

    private:
        size_t mSize{};
        std::variant<std::unique_ptr<T[], Deleter>, T *> mData{};
    };

    export template<typename T, size_t N>
    class FixedBuffer : public IBuffer<T> {
    public:
        virtual ~FixedBuffer() override = default;

        FixedBuffer(ResourceState::Uninitialized) {}

        FixedBuffer(ResourceState::DefaultInitialized)
            : mData{} {}

        FixedBuffer(const FixedBuffer &) = delete;

        FixedBuffer(const std::array<T, N> &data)
            : mData(data) {}

        FixedBuffer(const T (&data)[N]) {
            std::copy_n(data, N, mData.data());
        }

        FixedBuffer(std::array<T, N> &&data)
            : mData(std::move(data)) {}

        virtual T *Data() const override {
            return const_cast<T*>(mData.data());
        }

        virtual size_t Size() const override {
            return N;
        }

    private:
        std::array<T, N> mData;
    };

    export template<typename T>
    Ref<IBuffer<T>> MakeBufferRef(size_t size, ResourceState::Uninitialized) {
        return MakeRef<DynamicBuffer<T>>(size, ResourceState::Uninitialized{});
    }

    export template<typename T>
    Ref<IBuffer<T>> MakeBufferRef(size_t size, ResourceState::DefaultInitialized) {
        return MakeRef<DynamicBuffer<T>>(size, ResourceState::DefaultInitialized{});
    }

    export template<typename T, size_t N>
    Ref<IBuffer<T>> MakeBufferRef(ResourceState::Uninitialized) {
        return MakeRef<FixedBuffer<T, N>>(ResourceState::Uninitialized{});
    }

    export template<typename T, size_t N>
    Ref<IBuffer<T>> MakeBufferRef(ResourceState::DefaultInitialized) {
        return MakeRef<FixedBuffer<T, N>>(ResourceState::DefaultInitialized{});
    }
}
