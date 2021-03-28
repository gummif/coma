#include <cassert>
#include <utility>
#include <functional>
#include <asio/strand.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

namespace coma
{
namespace detail
{
    template<class T>
    struct is_awaitable : std::false_type {};
    template<class U, class E>
    struct is_awaitable<asio::awaitable<U, E>> : std::true_type {};
}

template<class T, class Executor>
class stranded
{
public:
    using executor_type = Executor;
    using value_type = T;

    explicit stranded(const executor_type& ex)
        : m_strand{ex}
        , m_value{}
    {}

    template<class... Args>
    explicit stranded(const executor_type& ex, std::in_place_t, Args&&... args)
        : m_strand{ex}
        , m_value(std::forward<Args>(args)...)
    {}

    template<class F, class Token = const asio::use_awaitable_t<>&, class FR = std::invoke_result_t<F, T&>>
        requires(detail::is_awaitable<FR>::value)
    [[nodiscard]]
    auto invoke(F&& f, Token&& token = asio::use_awaitable)
    {
        return asio::co_spawn(m_strand,
            [this, f = std::forward<F>(f)]() mutable -> asio::awaitable<typename FR::value_type> {
                co_return co_await std::invoke(std::move(f), m_value);
            }, std::forward<Token>(token));
    }

    template<class F, class Token = const asio::use_awaitable_t<>&, class FR = std::invoke_result_t<F, T&>>
        requires(!detail::is_awaitable<FR>::value)
    [[nodiscard]]
    auto invoke(F&& f, Token&& token = asio::use_awaitable)
    {
        return asio::co_spawn(m_strand,
            [this, f = std::forward<F>(f)]() mutable -> asio::awaitable<FR> {
                co_return std::invoke(std::move(f), m_value);
            }, std::forward<Token>(token));
    }

private:
    asio::strand<executor_type> m_strand;
    value_type m_value;
};

} // namespace coma
