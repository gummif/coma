#pragma once

#include <asio/basic_waitable_timer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/redirect_error.hpp>

namespace coma
{

// not thread-safe
template<class Executor>
class async_cond_var
{
    using clock = std::chrono::steady_clock;
    using timer = asio::basic_waitable_timer<
        clock, asio::wait_traits<clock>, Executor>;
public:
    using executor_type = Executor;

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

    [[nodiscard]]
    asio::awaitable<void> async_wait()
    {
        asio::error_code ec;
        co_await m_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        if (ec != asio::error::operation_aborted)
            throw asio::system_error(ec);
    }

    template<class Predicate>
    [[nodiscard]]
    asio::awaitable<void> async_wait(Predicate pred)
    {
        while (!pred())
            co_await async_wait();
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
};

} // namespace coma
