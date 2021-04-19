#pragma once

#include <coma/detail/core_async.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>

namespace coma
{

namespace detail
{

template<class Handler, class Timer>
class base_wait_op : public netext::async_base<Handler, typename Timer::executor_type>
{
protected:
    Timer& timer;

public:
    template<class H>
    base_wait_op(H&& h, Timer& tp)
        : netext::async_base<Handler, typename Timer::executor_type>(std::forward<H>(h), tp.get_executor())
        , timer{tp}
    {
    }
};


template<class Handler, class Timer>
class wait_op : public base_wait_op<Handler, Timer>
{
public:
    template<class H>
    wait_op(H&& h, Timer& tp)
        : base_wait_op<Handler, Timer>(std::forward<H>(h), tp)
    {
        this->timer.async_wait(std::move(*this));
    }

    void operator()(boost::system::error_code ec)
    {
        if (ec == net::error::operation_aborted)
            ec = {};
        this->complete_now(ec);
    }
};

struct run_wait_op
{
    template<class Handler, class Timer>
    void operator()(Handler&& h, Timer* t)
    {
        wait_op<typename std::decay<Handler>::type, Timer>(
                std::forward<Handler>(h), *t);
    }
};

template<class Handler, class Timer, class Predicate>
class wait_pred_op : public base_wait_op<Handler, Timer>
{
    Predicate pred;

public:
    template<class H, class P>
    wait_pred_op(H&& h, Timer& tp, P&& p)
        : base_wait_op<Handler, Timer>(std::forward<H>(h), tp)
        , pred{std::forward<P>(p)}
    {
        (*this)(boost::system::error_code{}, false);
    }

    void operator()(boost::system::error_code ec, bool continuation = true)
    {
        if (ec == net::error::operation_aborted)
            ec = {};
        if (ec || pred())
            this->complete(continuation, ec);
        else
            this->timer.async_wait(std::move(*this));
    }
};

struct run_wait_pred_op
{
    template<class Handler, class Timer, class Predicate>
    void operator()(Handler&& h, Timer* t, Predicate&& pred)
    {
        wait_pred_op<typename std::decay<Handler>::type,
                     Timer,
                     typename std::decay<Predicate>::type>(
                std::forward<Handler>(h), *t,
                std::forward<Predicate>(pred));
    }
};

} // namespace detail
} // namespace coma
