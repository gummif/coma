#include <coma/async_timed_cond_var.hpp>
#include <coma/co_lift.hpp>
#include <test_util.hpp>

TEST_CASE("async_timed_cond_var ctor", "[async_timed_cond_var]")
{
    asio::io_context ctx;
    coma::async_timed_cond_var cv{ctx.get_executor()};
}

TEST_CASE("async_timed_cond_var wait", "[async_timed_cond_var]")
{
    asio::io_context ctx;
    coma::async_timed_cond_var cv{ctx.get_executor()};

    bool done = false;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        co_await cv.async_wait();
        done = true;
    }, asio::detached);
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        cv.notify_one();
        co_return;
    }, asio::detached);

    ctx.run();
    CHECK(done); 
}

TEST_CASE("async_timed_cond_var wait many fifo", "[async_timed_cond_var]")
{
    asio::io_context ctx;
    coma::async_timed_cond_var cv{ctx.get_executor()};

    int done = 0;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        co_await cv.async_wait();
        CHECK(done == 0);
        done = 1;
    }, asio::detached);
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        co_await cv.async_wait();
        CHECK(done == 1);
        done = 2;
    }, asio::detached);
    asio::co_spawn(ctx, coma::co_lift([&]{ cv.notify_all(); }), asio::detached);

    ctx.run();
    CHECK(done == 2); 
}

TEST_CASE("async_timed_cond_var wait many fifo one", "[async_timed_cond_var]")
{
    asio::io_context ctx;
    coma::async_timed_cond_var cv{ctx.get_executor()};

    int done = 0;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        co_await cv.async_wait();
        CHECK(done == 0);
        done = 1;
    }, asio::detached);
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        co_await cv.async_wait();
        CHECK(done == 1);
        done = 2;
    }, asio::detached);
    ctx.poll();
    CHECK(done == 0);
    cv.notify_one();
    ctx.poll();
    CHECK(done == 1);
    cv.notify_one();
    ctx.poll();
    CHECK(done == 2);
}


TEST_CASE("async_timed_cond_var wait many pred", "[async_timed_cond_var]")
{
    asio::io_context ctx;
    coma::async_timed_cond_var cv{ctx.get_executor()};

    int done = 0;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        TEST_LOG("before await");
        co_await cv.async_wait(coma::logged_fn([&] { return done == 2; }));
        TEST_LOG("after await");
        CHECK(done == 2);
        done = 3;
    }, asio::detached);
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
        TEST_LOG("before await");
        co_await cv.async_wait(coma::logged_fn([&] { return done == 1; }));
        TEST_LOG("after await");
        CHECK(done == 1);
        done = 2;
        cv.notify_all();
    }, asio::detached);

    asio::co_spawn(ctx, coma::co_lift([&]{ cv.notify_all(); }), asio::detached);
    ctx.poll();
    CHECK(done == 0);

    asio::co_spawn(ctx, coma::co_lift([&]{
        done = 1;
        cv.notify_all();
    }), asio::detached);

    ctx.run();
    CHECK(done == 3);
}
