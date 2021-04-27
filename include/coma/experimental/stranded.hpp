#pragma once

#include <coma/detail/core_async.hpp>
#if defined(COMA_COROUTINES)

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cassert>
#include <coma/co_lift.hpp>
#include <functional>
#include <utility>

namespace coma {
namespace detail {
template<class T>
struct is_awaitable : std::false_type
{
};
template<class U, class E>
struct is_awaitable<boost::asio::awaitable<U, E>> : std::true_type
{
};
} // namespace detail

template<class T, class Executor>
class stranded
{
public:
	using executor_type = Executor;
	using value_type = T;

	explicit stranded(const executor_type& ex)
		: m_strand{ex}
		, m_value{}
	{
	}

	template<class... Args>
	explicit stranded(const executor_type& ex, std::in_place_t, Args&&... args)
		: m_strand{ex}
		, m_value(std::forward<Args>(args)...)
	{
	}

	template<class F, class Token = const boost::asio::use_awaitable_t<>&,
			 class FR = std::invoke_result_t<F, T&>>
	requires(detail::is_awaitable<FR>::value) COMA_NODISCARD
		auto invoke(F&& f, Token&& token = boost::asio::use_awaitable)
	{
		return boost::asio::co_spawn(m_strand, std::invoke(std::forward<F>(f), m_value),
									 std::forward<Token>(token));
	}

	template<class F, class Token = const boost::asio::use_awaitable_t<>&,
			 class FR = std::invoke_result_t<F, T&>>
	requires(!detail::is_awaitable<FR>::value) COMA_NODISCARD
		auto invoke(F&& f, Token&& token = boost::asio::use_awaitable)
	{
		return boost::asio::co_spawn(m_strand, co_lift([this, f = (F &&) f]() mutable {
										 return std::move(f)(m_value);
									 }),
									 std::forward<Token>(token));
	}

private:
	boost::asio::strand<executor_type> m_strand;
	value_type m_value;
};

} // namespace coma

#endif
