#pragma once

#include <coma/detail/core_async.hpp>
#include <coma/detail/wait_ops.hpp>
#include <coma/semaphore_guards.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

#include <cassert>
#include <cinttypes>

namespace coma {

// TODO time compared to simple co_spawn
// NOTE: this function must be co_await-ed directly called, may otherwise result in
// use-after-free errors
template<class Executor, class F>
COMA_NODISCARD auto co_dispatch(Executor ex, F&& f) -> net::awaitable<typename
std::invoke_result_t<F&&>::value_type>
{
	//auto c = co_await net::this_coro::executor;
	//if (ex == c)
	if (ex == co_await net::this_coro::executor)
	{
		puts("== dispatch same");
		co_return co_await std::forward<F>(f)();
	}
	else
	{
		puts("== dispatch spawn");
		co_return co_await net::co_spawn(ex, std::forward<F>(f), net::use_awaitable);
	}
}

// thread-safe variant
class async_semaphore_s
{
	using timer_executor_type = typename net::steady_timer::executor_type;
public:
	using executor_type = net::strand<timer_executor_type>;

	template<class Ex>
	explicit async_semaphore_s(const Ex& ex, std::ptrdiff_t init)
		: m_timer{ex, net::steady_timer::time_point::max()}
		, m_strand{ex}
		, m_counter{init}
	{
		assert(0 <= m_counter);
				std::cout << "ctor address = " << static_cast<void*>(this) << std::endl;
	}

	const executor_type& get_executor() const { return m_strand; }

	COMA_NODISCARD net::awaitable<void> async_acquire()
	{
				puts("== timer dispatching");
		co_await co_dispatch(m_strand, [this]() -> net::awaitable<void> {
				puts("== timer async wait");
				std::cout << "caq address = " << static_cast<void*>(this) << std::endl;
			assert(m_counter >= 0);
			if (m_counter == 0)
			{
				boost::system::error_code ec;
				co_await m_timer.async_wait(net::redirect_error(net::use_awaitable, ec));
				assert(m_counter > 0);
				puts("== timer async wait done");
			}
			--m_counter;
		});
	}

	COMA_NODISCARD net::awaitable<bool> async_try_acquire()
	{
		co_return co_await co_dispatch(m_strand, [this]() -> net::awaitable<bool> {
			assert(m_counter >= 0);
			if (m_counter == 0)
			{
				co_return false;
			}
			--m_counter;
			co_return true;
		});
	}

	// TODO ac for, ac until possible?
	// cancellation?

	COMA_NODISCARD net::awaitable<void> async_release()
	{
		co_await co_dispatch(m_strand, [this]() -> net::awaitable<void> {
				puts("== timer cancel");
				std::cout << "release address = " << static_cast<void*>(this) << std::endl;
			++m_counter;
			m_timer.cancel_one();
			co_return;
		});
	}

	COMA_NODISCARD net::awaitable<void> async_release(std::ptrdiff_t n)
	{
		assert(n >= 0);
		co_await co_dispatch(m_strand, [this, n]() mutable -> net::awaitable<void> {
			m_counter += n;
			for (; n > 0; --n)
			{
				m_timer.cancel_one();
			}
			co_return;
		});
	}

private:
	net::steady_timer m_timer;
	net::strand<timer_executor_type> m_strand;
	std::ptrdiff_t m_counter;
};

} // namespace coma
