#pragma once

#include <coma/detail/core_async.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>

namespace coma {

enum class cv_status
{
	no_timeout,
	timeout
};

namespace detail {

// there are 4 variants:
// no timeout - no predicate - returns error_code
// no timeout - with predicate - returns error_code
// with timeout - no predicate - returns error_code + cv_status
// with timeout - with predicate - returns error_code + bool

template<class Handler, class Impl>
class base_wait_until_op : public netext::async_base<Handler, typename Impl::executor_type>
{
	using time_point = typename Impl::time_point;
protected:
	struct remove_on_delete
	{
		time_point endtime;
		std::shared_ptr<void> time_points;
		void operator()(std::vector<time_point>* v) const noexcept
		{
			Impl::remove_time_point(*v, endtime);
		}
	};

	Impl& impl;
	time_point endtime;
	std::unique_ptr<std::vector<time_point>, remove_on_delete> endtime_guard;

	void add_time_point()
	{
		impl.add_time_point(endtime);
		endtime_guard = std::unique_ptr<std::vector<time_point>, remove_on_delete>(impl.time_points.get(), remove_on_delete{endtime, impl.time_points});
	}

	void do_complete_now(std::true_type, boost::system::error_code ec, bool p)
	{
		this->complete_now(ec, p);
	}
	void do_complete_now(std::false_type, boost::system::error_code ec, bool)
	{
		this->complete_now(ec);
	}
	void do_complete_now(std::true_type, boost::system::error_code ec, cv_status s)
	{
		this->complete_now(ec, s);
	}
	void do_complete_now(std::false_type, boost::system::error_code ec, cv_status)
	{
		this->complete_now(ec);
	}
	void do_complete_later(std::true_type, boost::system::error_code ec, bool p)
	{
		this->complete(false, ec, p);
	}
	void do_complete_later(std::false_type, boost::system::error_code ec, bool)
	{
		this->complete(false, ec);
	}
	void do_complete_later(std::true_type, boost::system::error_code ec, cv_status s)
	{
		this->complete(false, ec, s);
	}
	void do_complete_later(std::false_type, boost::system::error_code ec, cv_status)
	{
		this->complete(false, ec);
	}

public:
	template<class H>
	base_wait_until_op(H&& h, Impl& i, time_point et)
		: netext::async_base<Handler, typename Impl::executor_type>(std::forward<H>(h),
																	 i.timer.get_executor())
		, impl{i}
		, endtime{et}
	{
		net::post(this->get_executor(), []{ puts("rass c");});
	}

};

template<class Handler, class Impl, bool WithTimeout>
class wait_until_op : public base_wait_until_op<Handler, Impl>
{
public:
	using time_point = typename Impl::time_point;
	template<class H>
	wait_until_op(H&& h, Impl& i, time_point et)
		: base_wait_until_op<Handler, Impl>(std::forward<H>(h), i, et)
	{
		if (this->impl.stopped)
		{
			this->do_complete_later(std::integral_constant<bool, WithTimeout>{}, net::error::operation_aborted, cv_status::no_timeout);
			return;
		}
		this->add_time_point();
		this->impl.update_expire_time();
		// wait for something (timeout, signal, cancellation)
		this->impl.timer.async_wait(std::move(*this));
	}

	void operator()(boost::system::error_code ec)
	{
		if (ec == net::error::operation_aborted)
			ec = {};
		if (this->impl.stopped)
			ec = net::error::operation_aborted;
		this->endtime_guard.reset();
		const auto status = !WithTimeout || ec || Impl::clock::now() <= this->endtime ? cv_status::no_timeout : cv_status::timeout;
		this->do_complete_now(std::integral_constant<bool, WithTimeout>{}, ec, status);
	}
};

template<bool WithTimeout>
struct run_wait_until_op
{
	template<class Handler, class Impl>
	void operator()(Handler&& h, Impl* i, typename Impl::time_point et)
	{
		wait_until_op<typename std::decay<Handler>::type, Impl, WithTimeout>(std::forward<Handler>(h), *i, et);
	}
};

template<class Handler, class Impl, class Predicate, bool WithTimeout>
class wait_until_pred_op : public base_wait_until_op<Handler, Impl>
{
	Predicate pred;

public:
	using time_point = typename Impl::time_point;
	template<class H, class P>
	wait_until_pred_op(H&& h, Impl& i, time_point et, P&& p)
		: base_wait_until_op<Handler, Impl>(std::forward<H>(h), i, et)
		, pred{std::forward<P>(p)}
	{
		if (this->impl.stopped)
		{
			this->do_complete_later(std::integral_constant<bool, WithTimeout>{}, net::error::operation_aborted, false);
			return;
		}
		this->add_time_point();
		// must be posted such that there is no suspension point
		// between pred() == true and calling the completion handler
		net::post(std::move(*this));
	}

	void operator()(boost::system::error_code ec = boost::system::error_code{})
	{
		if (ec == net::error::operation_aborted)
			ec = {};
		if (this->impl.stopped)
			ec = net::error::operation_aborted;
		if (ec)
		{
			this->endtime_guard.reset();
			this->do_complete_now(std::integral_constant<bool, WithTimeout>{}, ec, false);
		}
		else if (pred())
		{
			this->endtime_guard.reset();
			this->do_complete_now(std::integral_constant<bool, WithTimeout>{}, ec, true);
		}
		else if (WithTimeout && Impl::clock::now() > this->endtime)
		{
			this->endtime_guard.reset();
			this->do_complete_now(std::integral_constant<bool, WithTimeout>{}, ec, false);
		}
		else
		{
			this->impl.update_expire_time();
			// wait for something (timeout, signal, cancellation)
			this->impl.timer.async_wait(std::move(*this));
		}
	}
};

template<bool WithTimeout>
struct run_wait_until_pred_op
{
	template<class Handler, class Impl, class Predicate>
	void operator()(Handler&& h, Impl* i, typename Impl::time_point et, Predicate&& pred)
	{
		wait_until_pred_op<typename std::decay<Handler>::type, Impl,
					 typename std::decay<Predicate>::type, WithTimeout>(std::forward<Handler>(h), *i, et,
														   std::forward<Predicate>(pred));
	}
};

} // namespace detail
} // namespace coma
