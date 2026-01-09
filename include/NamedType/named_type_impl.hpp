#ifndef named_type_impl_h
#define named_type_impl_h

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

// C++17 detection
#if defined(_MSC_VER) && (defined(_HAS_CXX17) && _HAS_CXX17)
#    define FLUENT_CPP17_PRESENT 1
#elif __cplusplus >= 201703L
#    define FLUENT_CPP17_PRESENT 1
#else
#    define FLUENT_CPP17_PRESENT 0
#endif

#if defined(__STDC_HOSTED__)
#    define FLUENT_HOSTED 1
#else
#    define FLUENT_HOSTED 0
#endif

// Use [[nodiscard]] if available
#ifndef FLUENT_NODISCARD_PRESENT
#    define FLUENT_NODISCARD_PRESENT FLUENT_CPP17_PRESENT
#endif

#if FLUENT_NODISCARD_PRESENT
#    define FLUENT_NODISCARD [[nodiscard]]
#else
#    define FLUENT_NODISCARD
#endif

// Enable empty base class optimization with multiple inheritance on Visual Studio.
#if defined(_MSC_VER) && _MSC_VER >= 1910
#    define FLUENT_EBCO __declspec(empty_bases)
#else
#    define FLUENT_EBCO
#endif

#if defined(__clang__) || defined(__GNUC__)
#   define IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_BEGIN                                                                \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Weffc++\"")
#   define IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_END _Pragma("GCC diagnostic pop")
#else
#   define IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_BEGIN /* Nothing */
#   define IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_END   /* Nothing */
#endif

namespace fluent
{

template <typename T>
using IsNotReference = typename std::enable_if<!std::is_reference<T>::value, void>::type;

// Brace initialization T{args...} is ill-formed if narrowing.
// See https://en.cppreference.com/w/cpp/language/aggregate_initialization.html
// "If that initializer is of syntax (1), and a narrowing conversion is required to convert the expression, the program is ill-formed."
template <typename T, typename... Args>
concept NonNarrowingConstructible = requires { T{std::declval<Args>()...}; };

// Safe integer-to-floating-point conversion: integers <= 32 bits fit exactly in double's 53-bit mantissa.
// The C++ standard treats int->float as narrowing for non-constant expressions, but this is overly
// conservative for small integers to double.
template <typename From, typename To>
concept SafeIntToFloat =
    std::is_integral_v<std::remove_cvref_t<From>> &&
    std::is_floating_point_v<std::remove_cvref_t<To>> &&
    sizeof(std::remove_cvref_t<From>) <= 4 &&
    sizeof(std::remove_cvref_t<To>) >= 8;

template <typename T, typename Parameter, template <typename> class... Skills>
class FLUENT_EBCO NamedType : public Skills<NamedType<T, Parameter, Skills...>>...
{
public:
    using UnderlyingType = T;

    // constructor
    NamedType()  = default;

    explicit constexpr NamedType(T const& value) noexcept(std::is_nothrow_copy_constructible<T>::value) : value_{value}
    {
    }

    template <typename T_ = T, typename = IsNotReference<T_>>
    explicit constexpr NamedType(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value)
        : value_{std::move(value)}
    {
    }

    // Forwarding constructor for multi-arg construction or single-arg conversion.
    // Requires:
    //   1. Either 2+ args, OR 1 arg of a type different from T (avoids ambiguity with copy/move ctors)
    //   2. NonNarrowingConstructible: rejects narrowing conversions (e.g. uint64_t -> uint32_t)
    //      by checking if brace-init T{args...} is well-formed
    template <typename... Args>
      requires (sizeof...(Args) > 1 || (sizeof...(Args) == 1 && !std::is_same_v<std::decay_t<Args>..., T>))
            && NonNarrowingConstructible<T, Args...>
    explicit constexpr NamedType(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
      : value_{std::forward<Args>(args)...}
    {
    }

    // Safe int-to-float constructor: allows small integers (<=32 bit) to construct floating-point T.
    // Uses parentheses init to bypass brace-init narrowing rules which are overly strict for this case.
    template <typename Arg>
      requires SafeIntToFloat<Arg, T>
            && (!NonNarrowingConstructible<T, Arg>)
    explicit constexpr NamedType(Arg&& arg) noexcept(std::is_nothrow_constructible_v<T, Arg>)
      : value_(std::forward<Arg>(arg))
    {
    }

    // get
    FLUENT_NODISCARD constexpr T& get() noexcept
    {
        return value_;
    }

    FLUENT_NODISCARD constexpr std::remove_reference_t<T> const& get() const noexcept
    {
        return value_;
    }

    // conversions
    using ref = NamedType<T&, Parameter, Skills...>;
    operator ref()
    {
        return ref(value_);
    }

    struct argument
    {
       NamedType operator=(T&& value) const
       {
           IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_BEGIN

           return NamedType(std::forward<T>(value));

           IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_END
       }
        // Rejects narrowing conversions (e.g. uint64_t -> uint32_t)
        template <typename U>
          requires NonNarrowingConstructible<T, U>
        NamedType operator=(U&& value) const
        {
            IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_BEGIN

            return NamedType(std::forward<U>(value));

            IGNORE_SHOULD_RETURN_REFERENCE_TO_THIS_END
        }

        argument() = default;
        argument(argument const&) = delete;
        argument(argument&&) = delete;
        argument& operator=(argument const&) = delete;
        argument& operator=(argument&&) = delete;
    };

private:
    T value_;
};

} // namespace fluent

#endif /* named_type_impl_h */
