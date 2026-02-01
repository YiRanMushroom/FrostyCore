export module Core.Utilities:TypeTraits;

namespace
Engine {
    template<typename T, typename U>
    concept IsImplicitlyConvertibleTo = requires(T *t, U *u) {
        u = t;
    };

    template<typename T, typename U>
    concept IsExplicitlyConvertibleTo = requires(T *t) {
        static_cast<U *>(t);
    };

    template<typename T, typename U>
    T* ImplicitCast(U* u) requires IsImplicitlyConvertibleTo<U, T> {
        return u;
    }
}
