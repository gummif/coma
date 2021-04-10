#include <coma/stranded.hpp>
#include <test_util.hpp>
#include <vector>

TEST_CASE("stranded simple invoke", "[stranded]")
{
    boost::asio::io_context ctx;
    coma::stranded<std::vector<int>, typename boost::asio::io_context::executor_type> st{ctx.get_executor()};

    boost::asio::co_spawn(ctx, [&]() -> boost::asio::awaitable<void>
    {
        co_await st.invoke([](auto& v) { CHECK(v.size() == 0); });
        co_await st.invoke([](auto& v) { v.push_back(1); });
        st.invoke([](auto& v) { CHECK(v.size() == 1); }, boost::asio::detached);
    }, boost::asio::detached);

    ctx.run();
}

TEST_CASE("stranded await invoke", "[stranded]")
{
    boost::asio::io_context ctx;
    coma::stranded<boost::asio::steady_timer, typename boost::asio::io_context::executor_type> st{ctx.get_executor(), std::in_place, ctx.get_executor()};

    boost::asio::co_spawn(ctx, [&]() -> boost::asio::awaitable<void>
    {
        co_await st.invoke([](auto& v) { v.expires_after(std::chrono::milliseconds{2}); });
        co_await st.invoke([](auto& v) { return v.async_wait(boost::asio::use_awaitable); });
    }, boost::asio::detached);

    ctx.run();
}