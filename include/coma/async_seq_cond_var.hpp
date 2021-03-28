#include <cassert>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/redirect_error.hpp>

namespace coma
{

// not thread-safe
// TODO make template
class async_seq_cond_var
{
public:
    using executor_type = typename asio::steady_timer::executor_type;

    explicit async_seq_cond_var(const executor_type& ex)
        : m_timer{ex, asio::steady_timer::time_point::max()}
    {
    }
    explicit async_seq_cond_var(executor_type&& ex)
        : m_timer{std::move(ex), asio::steady_timer::time_point::max()}
    {
    }

    [[nodiscard]] asio::awaitable<void> async_wait()
    {
        asio::error_code ec;
        co_await m_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
        if (ec != asio::error::operation_aborted)
            throw asio::system_error(ec);
    }

    void notify_one()
    {
        m_timer.cancel_one();
    }

    void notify_all()
    {
        m_timer.cancel();
    }

private:
    asio::steady_timer m_timer;
};

} // namespace coma
