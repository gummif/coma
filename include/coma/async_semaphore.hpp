#pragma once

#include <coma/detail/core_async.hpp>
#include <coma/detail/wait_ops.hpp>
#include <coma/semaphore_guards.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

#include <cassert>
#include <cinttypes>

namespace coma {

namespace detail {

struct acq_pred
{
	std::ptrdiff_t* counter;
	bool operator()() noexcept
	{
		assert(*counter >= 0);
		if (*counter == 0)
		{
			return false;
		}
		// successful completion handler called
		// if and only if pred() is true
		--(*counter);
		return true;
	}
};

struct acq_pred_n
{
	std::ptrdiff_t* counter;
	std::ptrdiff_t n;
	bool operator()() noexcept
	{
		assert(*counter >= 0);
		if (*counter < n)
		{
			return false;
		}
		// successful completion handler called
		// if and only if pred() is true
		*counter -= n;
		return true;
	}
};

} // namespace detail

template<class Executor COMA_SET_DEFAULT_IO_EXECUTOR>
class async_semaphore
{
	// TODO try implementing without a timer, since we can
	// never timeout, using a list of handlers functions
	using clock = std::chrono::steady_clock;
	using timer = net::basic_waitable_timer<clock, net::wait_traits<clock>, Executor>;
	using default_token = typename net::default_completion_token<Executor>::type;

public:
	using executor_type = Executor;
	template<class E>
	struct rebind_executor
	{
		using other = async_semaphore<E>;
	};

	explicit async_semaphore(const executor_type& ex, std::ptrdiff_t init)
		: m_timer{ex, timer::time_point::max()}
		, m_counter{init}
	{
		assert(0 <= m_counter);
	}
	explicit async_semaphore(executor_type&& ex, std::ptrdiff_t init)
		: m_timer{std::move(ex), timer::time_point::max()}
		, m_counter{init}
	{
		assert(0 <= m_counter);
	}
	~async_semaphore() = default;
	async_semaphore(const async_semaphore&) = delete;
	async_semaphore& operator=(const async_semaphore&) = delete;

	template<class CompletionToken = default_token>
	COMA_NODISCARD auto async_acquire(CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC
	{
		return net::async_initiate<CompletionToken, void(boost::system::error_code)>(
			detail::run_wait_pred_op{}, token, &m_timer, detail::acq_pred{&m_counter});
	}

	template<class CompletionToken = default_token>
	COMA_NODISCARD auto async_acquire_n(std::ptrdiff_t n,
										CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC
	{
		assert(n >= 0);
		return net::async_initiate<CompletionToken, void(boost::system::error_code)>(
			detail::run_wait_pred_op{}, token, &m_timer, detail::acq_pred_n{&m_counter, n});
	}

	COMA_NODISCARD bool try_acquire()
	{
		assert(m_counter >= 0);
		if (m_counter == 0)
		{
			return false;
		}
		--m_counter;
		return true;
	}

	void release()
	{
		++m_counter;
		m_timer.cancel_one();
	}

	void release(std::ptrdiff_t n)
	{
		assert(n >= 0);
		m_counter += n;
		if (n == 1)
			m_timer.cancel_one();
		else if (n > 1)
			m_timer.cancel(); // may result in spurious wakeup in acquire
	}

private:
	timer m_timer;
	std::ptrdiff_t m_counter;
};

} // namespace coma
