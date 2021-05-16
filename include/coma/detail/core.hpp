#pragma once

#include <type_traits>
#include <utility>

#if defined(_MSVC_LANG)
#define COMA_CPP_LANG _MSVC_LANG
#else
#define COMA_CPP_LANG __cplusplus
#endif

#if defined(_HAS_CXX14) && _HAS_CXX14 && COMA_CPP_LANG < 201402L
#undef COMA_CPP_LANG
#define COMA_CPP_LANG 201402L
#endif
#if defined(_HAS_CXX17) && _HAS_CXX17 && COMA_CPP_LANG < 201703L
#undef COMA_CPP_LANG
#define COMA_CPP_LANG 201703L
#endif

#if COMA_CPP_LANG >= 201402L
#define COMA_HAS_CPP14
#endif
#if COMA_CPP_LANG >= 201703L
#define COMA_HAS_CPP17
#endif

#if defined(COMA_HAS_CPP17)
#define COMA_NODISCARD [[nodiscard]]
#else
#define COMA_NODISCARD
#endif

#if defined(COMA_HAS_CPP14) && (!defined(_MSC_VER) || _MSC_VER > 1900)
#define COMA_CONSTEXPR_ASSERT
#endif
#if defined(COMA_HAS_CPP17)
#define COMA_INLINE_VAR inline
#define COMA_CONSTEXPR_IF constexpr
#else
#define COMA_INLINE_VAR
#define COMA_CONSTEXPR_IF
#endif

namespace coma {
namespace detail {

template<class T>
using void_t = void;

template<class T, class = void>
struct is_predicate : std::false_type
{
};

template<class T>
struct is_predicate<T, void_t<decltype(std::declval<T>()() == true)>> : std::true_type
{
};

template<class T, class U = T>
T exchange(T& a, U&& b)
{
	auto temp = std::move(a);
	a = std::forward<U>(b);
	return temp;
}

template<class F>
struct at_scope_exit
{
	F f;
	at_scope_exit(F&& g)
		: f{std::move(g)}
	{
	}
	~at_scope_exit() noexcept { f(); }
};

} // namespace detail
} // namespace coma
