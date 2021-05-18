#pragma once

#include <coma/detail/core_async.hpp>
#include <coma/detail/wait_until_ops.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

#include <vector>

namespace coma {

namespace detail {
template<class Executor>
struct cv_timed_impl
{
	using executor_type = Executor;
	using clock = std::chrono::steady_clock;
	using timer_type =
		boost::asio::basic_waitable_timer<clock, boost::asio::wait_traits<clock>, Executor>;
	using time_point = typename timer_type::time_point;

	timer_type timer;
	// sorted sorted vector of end times, optimized
	// for few waiting tasks
	// shared_ptr since we need to remove from the vector
	// in handler destructors, which can result in use-after-free
	// TODO can this be optimized to not use shared_ptr?
	std::shared_ptr<std::vector<time_point>> time_points{std::make_shared<std::vector<time_point>>()};
	bool stopped{false};

	explicit cv_timed_impl(const executor_type& ex)
		: timer{ex, timer_type::time_point::max()}
	{
	}
	explicit cv_timed_impl(executor_type&& ex)
		: timer{std::move(ex), timer_type::time_point::max()}
	{
	}

	void add_time_point(time_point endtime)
	{
		static_assert(std::is_trivially_copyable<time_point>::value, "");
		assert(time_points);
		auto& tp = *time_points;
		tp.push_back(endtime);
		// sort container
		for (size_t i = tp.size() - 1; i > 0 && tp[i - 1] > tp[i]; --i)
		{
			using std::swap;
			swap(tp[i - 1], tp[i]);
		}
	}

	static void remove_time_point(std::vector<time_point>& tp, time_point endtime) noexcept
	{
		auto it = std::find(tp.begin(), tp.end(), endtime);
		assert(it != tp.end());
		tp.erase(it);
	}

	void update_expire_time()
	{
		assert(time_points);
		auto& tp = *time_points;
		assert(!tp.empty());
		auto first = tp.front();
		if (timer.expiry() != first)
		{
			// will trigger all waiting
			timer.expires_at(first);
		}
	}
};
} // namespace detail

// not thread-safe
template<class Executor COMA_SET_DEFAULT_IO_EXECUTOR>
class async_cond_var_timed
{
	using impl_type = detail::cv_timed_impl<Executor>;
	using clock = std::chrono::steady_clock;
	using timer = typename impl_type::timer_type;
	using default_token = typename net::default_completion_token<Executor>::type;

public:
	using clock_type = typename timer::clock_type;
	using time_point = typename timer::time_point;
	using duration = typename timer::duration;
	using executor_type = Executor;
	template<class E>
	struct rebind_executor
	{
		using other = async_cond_var_timed<E>;
	};

	explicit async_cond_var_timed(const executor_type& ex)
		: m_impl{ex}
	{
	}
	explicit async_cond_var_timed(executor_type&& ex)
		: m_impl{std::move(ex)}
	{
	}
	~async_cond_var_timed() noexcept = default;
	async_cond_var_timed(const async_cond_var_timed&) = delete;
	async_cond_var_timed& operator=(const async_cond_var_timed&) = delete;

	template<class CompletionToken = default_token,
			 typename =
				 typename std::enable_if<!detail::is_predicate<CompletionToken>::value>::type>
	COMA_NODISCARD auto async_wait(CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC
	{
		return net::async_initiate<CompletionToken, void(boost::system::error_code)>(
			detail::run_wait_until_op<false>{}, token, &m_impl, time_point::max());
	}

	template<class Predicate, class CompletionToken = default_token,
			 typename = typename std::enable_if<detail::is_predicate<Predicate>::value>::type>
	COMA_NODISCARD auto async_wait(Predicate&& pred, CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC
	{
		return net::async_initiate<CompletionToken, void(boost::system::error_code)>(
			detail::run_wait_until_pred_op<false>{}, token, &m_impl, time_point::max(), std::forward<Predicate>(pred));
	}

	template<class CompletionToken = default_token,
			 typename =
				 typename std::enable_if<!detail::is_predicate<CompletionToken>::value>::type>
	COMA_NODISCARD auto async_wait_until(time_point endtime, CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC_AND(cv_status)
	{
		return net::async_initiate<CompletionToken, void(boost::system::error_code, cv_status)>(
			detail::run_wait_until_op<true>{}, token, &m_impl, endtime);
	}

	template<class Predicate, class CompletionToken = default_token,
			 typename =
				 typename std::enable_if<detail::is_predicate<Predicate>::value>::type>
	COMA_NODISCARD auto async_wait_until(time_point endtime, Predicate&& pred, CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC_AND(bool)
	{
		return net::async_initiate<CompletionToken, void(boost::system::error_code, bool)>(
			detail::run_wait_until_pred_op<true>{}, token, &m_impl, endtime, std::forward<Predicate>(pred));
	}

	template<class CompletionToken = default_token,
			 typename =
				 typename std::enable_if<!detail::is_predicate<CompletionToken>::value>::type>
	COMA_NODISCARD auto async_wait_for(duration dur, CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC_AND(cv_status)
	{
		return async_wait_until(clock_type::now() + dur, std::forward<CompletionToken>(token));
	}

	template<class Predicate, class CompletionToken = default_token,
			 typename =
				 typename std::enable_if<detail::is_predicate<Predicate>::value>::type>
	COMA_NODISCARD auto async_wait_for(duration dur, Predicate&& pred, CompletionToken&& token = default_token{})
		-> COMA_ASYNC_RETURN_EC_AND(bool)
	{
		return async_wait_until(clock_type::now() + dur, std::forward<Predicate>(pred), std::forward<CompletionToken>(token));
	}

	void notify_one() { m_impl.timer.cancel_one(); }

	void notify_all() { m_impl.timer.cancel(); }

	void stop()
	{
		m_impl.stopped = true;
		notify_all();
	}

	COMA_NODISCARD bool stopped() const noexcept
	{
		return m_impl.stopped;
	}

	void restart() { m_impl.stopped = false; }

	executor_type get_executor() { return m_impl.timer.get_executor(); }

private:
	impl_type m_impl;
};

} // namespace coma
