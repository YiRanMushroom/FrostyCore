export module Core.Utilities:MultiInterface;

import Core.Prelude;

import :TypeTraits;

namespace
Engine {
    template<typename Target, typename... Sources>
    constexpr bool ImplicitlyConvertibleFromAny = (IsImplicitlyConvertibleTo<Sources, Target> || ...);

    template<typename Source, typename... Targets>
    constexpr bool ImplicitlyConvertibleToAll = (IsImplicitlyConvertibleTo<Source, Targets> && ...);

    export template<typename... InterfaceTypes>
    struct MultiInterfaceBase {
        MultiInterfaceBase() = default;

        MultiInterfaceBase(const std::tuple<InterfaceTypes *...> &interfaces)
            : Interfaces(interfaces) {}

        std::tuple<InterfaceTypes *...> Interfaces;

        template<typename T> requires ImplicitlyConvertibleFromAny<T, InterfaceTypes...>
        T *GetInterface() const; // one of the InterfaceTypes must be implicitly convertible to T

        template<typename S> requires ImplicitlyConvertibleToAll<S, InterfaceTypes...>
        static MultiInterfaceBase FromDerivedType(S *derived) {
            return MultiInterfaceBase{
                std::make_tuple(ImplicitCast<InterfaceTypes>(derived)...)
            };
        }

        template<typename... OtherInterfaceTypes>
        static MultiInterfaceBase FromOtherInterfaceBase(const MultiInterfaceBase<OtherInterfaceTypes...> &other)
            requires ((ImplicitlyConvertibleFromAny<InterfaceTypes, OtherInterfaceTypes...>) && ...);

        [[nodiscard]] bool HasValue() const {
            return std::get<0>(Interfaces) != nullptr;
        }
    };

    template<typename T, typename... InterfaceTypes>
    consteval std::optional<size_t> FindConvertibleIndex() {
        if constexpr (sizeof...(InterfaceTypes) == 0) {
            return std::nullopt;
        } else {
            size_t index = 0;
            if (((IsImplicitlyConvertibleTo<InterfaceTypes, T> ? true : (index++, false)) || ...)) {
                return index;
            }
            return std::nullopt;
        }
    }

    template<typename... InterfaceTypes>
    template<typename T> requires ImplicitlyConvertibleFromAny<T, InterfaceTypes...>
    T *MultiInterfaceBase<InterfaceTypes...>::GetInterface() const {
        constexpr auto indexOpt = FindConvertibleIndex<T, InterfaceTypes...>();
        static_assert(indexOpt.has_value(), "No convertible interface found");
        constexpr size_t index = indexOpt.value();

        return ImplicitCast<T>(std::get<index>(Interfaces));
    }

    template<typename... InterfaceTypes>
    template<typename... OtherInterfaceTypes>
    MultiInterfaceBase<InterfaceTypes...> MultiInterfaceBase<InterfaceTypes...>::
    FromOtherInterfaceBase(const MultiInterfaceBase<OtherInterfaceTypes...> &other) requires ((
        ImplicitlyConvertibleFromAny
        <InterfaceTypes, OtherInterfaceTypes...>) && ...) {
        return MultiInterfaceBase{
            std::make_tuple(
                other.template GetInterface<InterfaceTypes>()...
            )
        };
    }

    export template<typename... InterfaceTypes>
    class MultiInterface; // forward declaration

    template<typename... InterfaceTypes>
    struct MultiInterfaceExtensions {
        // default implementation does nothing, specialize it for specific interfaces,
        // use deducing this to access the MultiInterfaceBase
    };


    export template<typename... InterfaceTypes>
    class MultiInterface : public MultiInterfaceExtensions<InterfaceTypes...> {
        friend MultiInterfaceExtensions<InterfaceTypes...>;

    public:
        friend MultiInterfaceBase<InterfaceTypes...>;

        MultiInterface() = default;

        MultiInterface(std::nullptr_t) {}

        template<typename S>
        MultiInterface(const S *derived) requires (ImplicitlyConvertibleToAll<S, InterfaceTypes...>)
            : mStoredInterfaces(MultiInterfaceBase<InterfaceTypes...>::FromDerivedType(derived)) {}

        template<typename... OtherInterfaceTypes>
        MultiInterface(const MultiInterfaceBase<OtherInterfaceTypes...> &other)
            requires ((ImplicitlyConvertibleFromAny<InterfaceTypes, OtherInterfaceTypes...>) && ...)
            : mStoredInterfaces(MultiInterfaceBase<InterfaceTypes...>::FromOtherInterfaceBase(other)) {}

        [[nodiscard]] bool HasValue() const {
            return mStoredInterfaces.HasValue();
        }

        explicit operator bool() const {
            return HasValue();
        }

        static MultiInterface CreateFromRawPointersUnsafe(InterfaceTypes *... pointers) {
            MultiInterface result;
            result.mStoredInterfaces.Interfaces = std::make_tuple(pointers...);
            return result;
        }

        template<typename T> requires (ImplicitlyConvertibleFromAny<T, InterfaceTypes...>)
        T *GetInterface() const {
            return mStoredInterfaces.template GetInterface<T>();
        }

        template<typename T>
        operator T *() requires (ImplicitlyConvertibleFromAny<T, InterfaceTypes...>) {
            return GetInterface<T>();
        }

        const MultiInterfaceBase<InterfaceTypes...> &GetBase() const {
            return mStoredInterfaces;
        }

    private:
        template<typename T>
        struct IntoImpl {
            static_assert(false, "IntoImpl not specialized for this type");
        };

        template<typename... Tps>
        struct IntoImpl<MultiInterface<Tps...>> {
            template<typename... CurrentTypes>
            static MultiInterface<Tps...> Invoke(
                const MultiInterface<CurrentTypes...> &self) {
                return MultiInterface<Tps...>{
                    MultiInterfaceBase<Tps...>::FromOtherInterfaceBase(
                        self.GetBase()
                    )
                };
            }
        };

    public:
        template<typename AnotherInterface>
        AnotherInterface Into() const {
            return IntoImpl<AnotherInterface>::Invoke(*this);
        }

    private:
        MultiInterfaceBase<InterfaceTypes...> mStoredInterfaces;
    };

    export template<typename... InterfaceTypes>
    bool operator==(const MultiInterface<InterfaceTypes...> &interface, std::nullptr_t) {
        return !interface.HasValue();
    }

    export template<typename... InterfaceTypes>
    bool operator==(std::nullptr_t, const MultiInterface<InterfaceTypes...> &interface) {
        return !interface.HasValue();
    }
}
