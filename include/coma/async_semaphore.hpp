#include <coma/acquire_guard.hpp>

#include <cassert>
#include <cinttypes>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/redirect_error.hpp>

#include <iostream>

namespace coma
{

    // not thread-safe
    class async_semaphore_st
    {
    public:
        using executor_type = typename asio::steady_timer::executor_type;

        explicit async_semaphore_st(const executor_type& ex, std::ptrdiff_t init)
            : m_timer{ex, asio::steady_timer::time_point::max()}
            , m_counter{init}
        {
            assert(0 <= m_counter);
        }

        [[nodiscard]] asio::awaitable<void> async_acquire()
        {
            assert(m_counter >= 0);
            if (m_counter == 0)
            {
                // TODO track waiting count? measure
                asio::error_code ec;
                co_await m_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
                assert(m_counter > 0);
            }
            --m_counter;
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

        // TODO ac for, ac until possible? yes, but wiht a cost, multiset
        // cancellation? no, only for cv, see condition_variable_any, requires spurios fails, call notify all on cancel

        void release()
        {
            ++m_counter;
            m_timer.cancel_one();
        }

        void release(std::ptrdiff_t n)
        {
            assert(n >= 0);
            m_counter += n;
            for (; n > 0; --n)
            {
                m_timer.cancel_one();
            }
        }

    private:
        asio::steady_timer m_timer;
        std::ptrdiff_t m_counter;
    };

    // TODO time compared to simple co_spawn
    // NOTE: this function must be co_await-ed directly called, may otherwise result in use-after-free errors
    template<class Executor, class F>
    [[nodiscard]] auto co_dispatch(Executor ex, F&& f) -> asio::awaitable<typename std::invoke_result_t<F&&>::value_type>
    {
        //auto c = co_await asio::this_coro::executor;
        //if (ex == c)
        if (ex == co_await asio::this_coro::executor)
        {
            puts("== dispatch same");
            co_return co_await std::forward<F>(f)();
        }
        else
        {
            puts("== dispatch spawn");
            co_return co_await asio::co_spawn(ex, std::forward<F>(f), asio::use_awaitable);
        }
    }

    // thread-safe variant
    class async_semaphore
    {
        using timer_executor_type = typename asio::steady_timer::executor_type;
    public:
        using executor_type = asio::strand<timer_executor_type>;

        template<class Ex>
        explicit async_semaphore(const Ex& ex, std::ptrdiff_t init)
            : m_timer{ex, asio::steady_timer::time_point::max()}
            , m_strand{ex}
            , m_counter{init}
        {
            assert(0 <= m_counter);
                    std::cout << "ctor address = " << static_cast<void*>(this) << std::endl;
        }

        const executor_type& get_executor() const { return m_strand; }

        [[nodiscard]] asio::awaitable<void> async_acquire()
        {
                    puts("== timer dispatching");
            co_await co_dispatch(m_strand, [this]() -> asio::awaitable<void> {
                    puts("== timer async wait");
                    std::cout << "caq address = " << static_cast<void*>(this) << std::endl;
                assert(m_counter >= 0);
                if (m_counter == 0)
                {
                    asio::error_code ec;
                    co_await m_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
                    assert(m_counter > 0);
                    puts("== timer async wait done");
                }
                --m_counter;
            });
        }

        [[nodiscard]] asio::awaitable<bool> async_try_acquire()
        {
            co_return co_await co_dispatch(m_strand, [this]() -> asio::awaitable<bool> {
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

        [[nodiscard]] asio::awaitable<void> async_release()
        {
            co_await co_dispatch(m_strand, [this]() -> asio::awaitable<void> {
                    puts("== timer cancel");
                    std::cout << "release address = " << static_cast<void*>(this) << std::endl;
                ++m_counter;
                m_timer.cancel_one();
                co_return;
            });
        }

        [[nodiscard]] asio::awaitable<void> async_release(std::ptrdiff_t n)
        {
            assert(n >= 0);
            co_await co_dispatch(m_strand, [this, n]() mutable -> asio::awaitable<void> {
                m_counter += n;
                for (; n > 0; --n)
                {
                    m_timer.cancel_one();
                }
                co_return;
            });
        }

    private:
        asio::steady_timer m_timer;
        asio::strand<timer_executor_type> m_strand;
        std::ptrdiff_t m_counter;
    };

} // namespace coma
