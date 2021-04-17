#pragma once

#include <coma/detail/core.hpp>
#include <coma/detail/wait_ops.hpp>
#include <coma/acquire_guard.hpp>
#include <coma/unique_acquire_guard.hpp>

#include <boost/asio/basic_waitable_timer.hpp>

#include <cassert>
#include <cinttypes>

namespace coma
{

namespace detail
{

struct acq_pred {
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

struct acq_pred_n {
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

template<class Executor = net::any_io_executor>
class async_semaphore
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
    [[nodiscard]] auto async_acquire(CompletionToken&& token = default_token{})
    {
        return net::async_initiate<
            CompletionToken,
            void(boost::system::error_code)>(
                detail::run_wait_pred_op{},
                token, &m_timer, detail::acq_pred{&m_counter});
    }

    template<class CompletionToken = default_token>
    [[nodiscard]] auto async_acquire_n(std::ptrdiff_t n, CompletionToken&& token = default_token{})
    {
        assert(n >= 0);
        return net::async_initiate<
            CompletionToken,
            void(boost::system::error_code)>(
                detail::run_wait_pred_op{},
                token, &m_timer, detail::acq_pred_n{&m_counter, n});
    }

    [[nodiscard]] bool try_acquire()
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

// TODO time compared to simple co_spawn
// NOTE: this function must be co_await-ed directly called, may otherwise result in use-after-free errors
/*template<class Executor, class F>
[[nodiscard]] auto co_dispatch(Executor ex, F&& f) -> net::awaitable<typename std::invoke_result_t<F&&>::value_type>
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
}*/

// thread-safe variant
/*class async_semaphore_s
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

    [[nodiscard]] net::awaitable<void> async_acquire()
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

    [[nodiscard]] net::awaitable<bool> async_try_acquire()
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

    [[nodiscard]] net::awaitable<void> async_release()
    {
        co_await co_dispatch(m_strand, [this]() -> net::awaitable<void> {
                puts("== timer cancel");
                std::cout << "release address = " << static_cast<void*>(this) << std::endl;
            ++m_counter;
            m_timer.cancel_one();
            co_return;
        });
    }

    [[nodiscard]] net::awaitable<void> async_release(std::ptrdiff_t n)
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
};*/

} // namespace coma
