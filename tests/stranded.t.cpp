#include <coma/stranded.hpp>
#include <asio/io_context.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <vector>
#include <catch2/catch.hpp>

TEST_CASE("stranded simple invoke", "[stranded]")
{

    asio::io_context ctx;
    coma::stranded<std::vector<int>, typename asio::io_context::executor_type> st{ctx.get_executor()};

    asio::co_spawn(ctx.get_executor(), [&]() -> asio::awaitable<void>
    {
        co_await st.invoke([](auto& v) { CHECK(v.size() == 0); });
        co_await st.invoke([](auto& v) { v.push_back(1); });
        st.invoke([](auto& v) { CHECK(v.size() == 1); }, asio::detached);
    }, asio::detached);

    ctx.run();
}

TEST_CASE("stranded await invoke", "[stranded]")
{

    asio::io_context ctx;
    coma::stranded<asio::steady_timer, typename asio::io_context::executor_type> st{ctx.get_executor(), std::in_place, ctx.get_executor()};

    asio::co_spawn(ctx.get_executor(), [&]() -> asio::awaitable<void>
    {
        co_await st.invoke([](auto& v) { v.expires_after(std::chrono::milliseconds{2}); });
        co_await st.invoke([](auto& v) { return v.async_wait(asio::use_awaitable); });
    }, asio::detached);

    ctx.run();
}