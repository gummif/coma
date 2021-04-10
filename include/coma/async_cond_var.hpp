#pragma once

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/stream_traits.hpp>

namespace coma
{

// not thread-safe
template<class Executor = boost::asio::any_io_executor>
class async_cond_var
{
    using clock = std::chrono::steady_clock;
    using timer = boost::asio::basic_waitable_timer<
        clock, boost::asio::wait_traits<clock>, Executor>;
public:
    using executor_type = Executor;
    template<class E>
    struct rebind_executor
    {
        using other = async_cond_var<E>;
    };

    explicit async_cond_var(const executor_type& ex)
        : m_timer{ex, timer::time_point::max()}
    {
    }
    explicit async_cond_var(executor_type&& ex)
        : m_timer{std::move(ex), timer::time_point::max()}
    {
    }
    ~async_cond_var() noexcept = default;
    async_cond_var(const async_cond_var&) = delete;
    async_cond_var(async_cond_var&&) = delete;
    async_cond_var& operator=(const async_cond_var&) = delete;
    async_cond_var& operator=(async_cond_var&&) = delete;

    template<class CompletionToken = boost::asio::default_completion_token_t<executor_type>>
    [[nodiscard]] auto async_wait(CompletionToken&& token = boost::asio::default_completion_token_t<executor_type>{});

    //todo enable if
    template<class Predicate, class CompletionToken>
    [[nodiscard]] auto async_wait(Predicate&& pred, CompletionToken&& token);

    void notify_one()
    {
        m_timer.cancel_one();
    }

    void notify_all()
    {
        m_timer.cancel();
    }

    executor_type get_executor() { return m_timer.get_executor(); }

private:
    timer m_timer;

    template<class Handler>
    class wait_op : public boost::beast::async_base<Handler, executor_type>
    {
        async_cond_var* cv;

    public:
        template<class H>
        wait_op(H&& h, async_cond_var* cvp)
            : boost::beast::async_base<Handler, executor_type>(std::forward<H>(h), cvp->get_executor())
            , cv{cvp}
        {
            (*this)();
        }

        void operator()()
        {
            cv->m_timer.async_wait(std::move(*this));
        }

        void operator()(boost::system::error_code ec)
        {
            if (ec == boost::asio::error::operation_aborted)
                ec = {};
            this->complete_now(ec);
        }
    };

    template<class Handler, class Predicate>
    class wait_pred_op : public boost::beast::stable_async_base<Handler, executor_type>
    {
        async_cond_var* cv;
        Predicate& pred;

    public:
        template<class H, class P>
        wait_pred_op(H&& h, async_cond_var* cvp, P&& p)
            : boost::beast::stable_async_base<Handler, executor_type>(std::forward<H>(h), cvp->get_executor())
            , cv{cvp}
            , pred{boost::beast::allocate_stable<Predicate>(*this, std::forward<P>(p))}
        {
            (*this)();
        }

        void operator()()
        {
            if (pred())
                this->complete(false, boost::system::error_code{});
            else
                cv->m_timer.async_wait(std::move(*this));
        }

        void operator()(boost::system::error_code ec)
        {
            if (ec == boost::asio::error::operation_aborted)
                ec = {};
            if (ec || pred())
                this->complete_now(ec);
            else
                cv->m_timer.async_wait(std::move(*this));
        }
    };

    struct run_wait_op
    {
        template<class Handler, class... Args>
        void operator()(Handler&& h, Args&&... args)
        {
            wait_op<typename std::decay<Handler>::type>(
                    std::forward<Handler>(h),
                    std::forward<Args>(args)...);
        }
    };
    template<class Predicate>
    struct run_wait_pred_op
    {
        template<class Handler, class... Args>
        void operator()(Handler&& h, Args&&... args)
        {
            wait_pred_op<typename std::decay<Handler>::type, Predicate>(
                    std::forward<Handler>(h),
                    std::forward<Args>(args)...);
        }
    };
};

template<class Executor>
template<class CompletionToken>
auto async_cond_var<Executor>::async_wait(CompletionToken&& token)
{
    return boost::asio::async_initiate<
        CompletionToken,
        void(boost::system::error_code)>(run_wait_op{}, token, this);
}

template<class Executor>
template<class Predicate, class CompletionToken>
auto async_cond_var<Executor>::async_wait(Predicate&& pred, CompletionToken&& token)
{
    return boost::asio::async_initiate<
        CompletionToken,
        void(boost::system::error_code)>(
            run_wait_pred_op<typename std::decay<Predicate>::type>{},
            token, this, std::forward<Predicate>(pred));
}

} // namespace coma
