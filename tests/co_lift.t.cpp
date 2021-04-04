#include <coma/co_lift.hpp>
#include <test_util.hpp>
#include <iostream>
#include <vector>

TEST_CASE("co_lift", "[co_lift]")
{
    asio::io_context ctx;
    std::vector<int> v;

    asio::co_spawn(ctx, coma::co_lift([&]
    {
        v.push_back(42);
    }), asio::detached);

    asio::co_spawn(ctx, [&]() -> asio::awaitable<void>
    {
        TEST_LOG("before await");
        auto val = co_await coma::co_lift([&, l = coma::logged()]()
        {
            TEST_LOG("inside lifted fn");
            v.push_back(43);
            return 0;
        });
        TEST_LOG("after await");
        CHECK(val == 0);
        TEST_LOG("before await");
        val = co_await coma::co_lift([&, l = coma::logged()]()
        {
            return 2;
        });
        TEST_LOG("after await");
        CHECK(val == 2);
    }, asio::detached);

    ctx.run();

    CHECK(v[0] == 42);
    CHECK(v[1] == 43);
}
