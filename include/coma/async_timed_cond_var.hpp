#pragma once

#include <asio/basic_waitable_timer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/redirect_error.hpp>
#include <set>

namespace coma
{

namespace detail
{
template<class F>
struct at_scope_exit
{
    F f;
    at_scope_exit(F&& g) : f{std::move(g)} { }
    ~at_scope_exit() noexcept { f(); }
};
}

enum class cv_status {
    no_timeout,
    timeout  
};

// not thread-safe
template<class Executor>
class async_timed_cond_var
{
    using clock = std::chrono::steady_clock;
    using timer = asio::basic_waitable_timer<
        clock, asio::wait_traits<clock>, Executor>;
public:
    using executor_type = Executor;
    using clock_type = typename timer::clock_type;
    using time_point = typename timer::time_point;
    using duration = typename timer::duration;

    explicit async_timed_cond_var(const executor_type& ex)
        : m_timer{ex, timer::time_point::max()}
    {
    }
    explicit async_timed_cond_var(executor_type&& ex)
        : m_timer{std::move(ex), timer::time_point::max()}
    {
    }
    ~async_timed_cond_var() noexcept = default;
    async_timed_cond_var(const async_timed_cond_var&) = delete;
    async_timed_cond_var(async_timed_cond_var&&) = delete;
    async_timed_cond_var& operator=(const async_timed_cond_var&) = delete;
    async_timed_cond_var& operator=(async_timed_cond_var&&) = delete;

    [[nodiscard]]
    asio::awaitable<void> async_wait()
    {
        asio::error_code ec;
        co_await m_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        if (ec && ec != asio::error::operation_aborted)
            throw asio::system_error(ec);
    }

    template<class Predicate>
    [[nodiscard]]
    asio::awaitable<void> async_wait(Predicate pred)
    {
        while (!pred())
            co_await async_wait();
    }

    [[nodiscard]]
    asio::awaitable<cv_status> async_wait_until(time_point endtime)
    {
        auto it = m_times.insert(endtime);
        detail::at_scope_exit se{[&] { m_times.erase(it); }};
        auto first = *m_times.begin();
        if (m_timer.expiry() != first)
            m_timer.expires_at(first);
        // wait for something (timeout, signal, cancellation)
        co_await async_wait();
        co_return clock_type::now() >= endtime
            ? cv_status::timeout
            : cv_status::no_timeout;
    }

    template<class Predicate>
    [[nodiscard]]
    asio::awaitable<bool> async_wait_until(time_point endtime, Predicate pred)
    {
        while (!pred())
        {
            if (co_await async_wait_until(endtime) == cv_status::timeout) {
                co_return pred();
            }
        }
        co_return true;
    }

    [[nodiscard]]
    asio::awaitable<cv_status> async_wait_for(duration dur)
    {
        return async_wait_until(clock_type::now() + dur);
    }

    template<class Predicate>
    [[nodiscard]]
    asio::awaitable<bool> async_wait_for(duration dur, Predicate pred)
    {
        return async_wait_until(clock_type::now() + dur, std::move(pred));
    }

    void notify_one()
    {
        m_timer.cancel_one();
    }

    void notify_all()
    {
        m_timer.cancel();
    }

    executor_type get_executor() const { return m_timer.get_executor(); }

private:
    timer m_timer;
    std::multiset<time_point> m_times; // TODO use set of pair<time_point, size_t>
};

} // namespace coma
