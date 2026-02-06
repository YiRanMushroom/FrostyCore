export module Core.Utilities:StringLiteral;

import Core.Prelude;

namespace
Engine {
    export template<size_t N>
    struct StringLiteral {
        constexpr StringLiteral(const char (&str)[N]) {
            std::copy_n(str, N, Value);
        }

        char Value[N];

    public:
        constexpr StringLiteral() {
            Value[0] = '\0';
        }

        constexpr std::string_view View() const {
            return std::string_view(Value, N - 1);
        }

        constexpr size_t Size() const {
            return N - 1;
        }

        template<size_t Begin, size_t End>
        constexpr auto Slice() const {
            static_assert(Begin <= End, "Begin must be less than or equal to End");
            static_assert(End <= N - 1, "End must be less than or equal to the size of the string literal");
            StringLiteral<End - Begin + 1> result{};
            for (size_t i = Begin; i < End; ++i) {
                result.Value[i - Begin] = Value[i];
            }
            result.Value[End - Begin] = '\0';
            return result;
        }

        constexpr StringLiteral(std::string_view sv) {
            std::copy_n(sv.data(), N, Value);
        }
    };

    export template<size_t N>
    StringLiteral(const char (&)[N]) -> StringLiteral<N>;

    export template<size_t N1, size_t N2>
    constexpr bool operator==(const StringLiteral<N1> &lhs, const StringLiteral<N2> &rhs) {
        return lhs.View() == rhs.View();
    }

    export template<size_t N1, size_t N2>
    constexpr bool operator!=(const StringLiteral<N1> &lhs, const StringLiteral<N2> &rhs) {
        return !(lhs == rhs);
    }

    export template<StringLiteral literal>
    constexpr auto operator""_sl() {
        return literal;
    }


    export template<size_t N1, size_t N2>
    constexpr auto operator+(const StringLiteral<N1> &lhs, const StringLiteral<N2> &rhs) {
        StringLiteral<N1 + N2 - 1> result{};
        std::copy_n(lhs.Value, N1 - 1, result.Value);
        std::copy_n(rhs.Value, N2, result.Value + N1 - 1);
        return result;
    }

    static_assert("Hello, World!"_sl.View() == "Hello, World!");
    static_assert(("Hello, "_sl + "World!"_sl).View() == "Hello, World!");
    static_assert("Hello, "_sl + "World!"_sl == "Hello, World!"_sl);
    static_assert("Hello world!"_sl.Slice<0, 5>().View() == "Hello");
}
