#include <coma/async_cond_var.hpp>
#include <coma/co_lift.hpp>
#include <test_util.hpp>

TEST_CASE("async_cond_var ctor", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var cv{ctx.get_executor()};
}

#if defined(COMA_ENABLE_COROUTINE_TESTS)

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using cond_var = boost::asio::use_awaitable_t<>::as_default_on_t<coma::async_cond_var<>>;

TEST_CASE("async_cond_var wait", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    cond_var cv{ctx.get_executor()};

    bool done = false;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait();
        done = true;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        cv.notify_one();
        co_return;
    }, boost::asio::detached);

    ctx.run();
    CHECK(done); 
}

TEST_CASE("async_cond_var wait many fifo", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var cv{ctx.get_executor()};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait(use_awaitable);
        CHECK(done == 0);
        done = 1;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait(use_awaitable);
        CHECK(done == 1);
        done = 2;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, coma::co_lift([&]{ cv.notify_all(); }), boost::asio::detached);

    ctx.run();
    CHECK(done == 2); 
}

TEST_CASE("async_cond_var wait many fifo one", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var cv{ctx.get_executor()};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait(use_awaitable);
        CHECK(done == 0);
        done = 1;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait(use_awaitable);
        CHECK(done == 1);
        done = 2;
    }, boost::asio::detached);
    ctx.poll();
    CHECK(done == 0);
    cv.notify_one();
    ctx.poll();
    CHECK(done == 1);
    cv.notify_one();
    ctx.poll();
    CHECK(done == 2);
}

TEST_CASE("async_cond_var wait many pred", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    cond_var cv{ctx.get_executor()};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        TEST_LOG("before await");
        co_await cv.async_wait(coma::logged_fn([&] { return done == 2; }));
        TEST_LOG("after await");
        CHECK(done == 2);
        done = 3;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        TEST_LOG("before await");
        co_await cv.async_wait(coma::logged_fn([&] { return done == 1; }), use_awaitable);
        TEST_LOG("after await");
        CHECK(done == 1);
        done = 2;
        cv.notify_all();
    }, boost::asio::detached);

    boost::asio::co_spawn(ctx, coma::co_lift([&]{ cv.notify_all(); }), boost::asio::detached);
    ctx.poll();
    CHECK(done == 0);

    boost::asio::co_spawn(ctx, coma::co_lift([&]{
        done = 1;
        cv.notify_all();
    }), boost::asio::detached);

    ctx.run();
    CHECK(done == 3);
}

#endif // defined(COMA_ENABLE_COROUTINE_TESTS)
