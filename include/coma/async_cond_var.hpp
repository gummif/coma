#pragma once

#include <coma/detail/core.hpp>
#include <coma/detail/wait_ops.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

namespace coma
{

template<class Executor = net::any_io_executor>
class async_cond_var
{
    using clock = std::chrono::steady_clock;
    using timer = net::basic_waitable_timer<
        clock, net::wait_traits<clock>, Executor>;
    using default_token = net::default_completion_token_t<Executor>;
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

    template<class CompletionToken = default_token,
             typename = std::enable_if_t<!detail::is_predicate<CompletionToken>::value>>
    [[nodiscard]] auto async_wait(CompletionToken&& token = default_token{})
    {
        return net::async_initiate<
            CompletionToken,
            void(boost::system::error_code)>(detail::run_wait_op{}, token, &m_timer);
    }

    template<class Predicate, class CompletionToken = default_token,
             typename = std::enable_if_t<detail::is_predicate<Predicate>::value>>
    [[nodiscard]] auto async_wait(Predicate&& pred, CompletionToken&& token = default_token{})
    {
        return net::async_initiate<
            CompletionToken,
            void(boost::system::error_code)>(
                detail::run_wait_pred_op{},
                token, &m_timer, std::forward<Predicate>(pred));
    }

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
};

} // namespace coma
