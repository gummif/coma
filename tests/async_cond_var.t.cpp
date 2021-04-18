#include <coma/async_cond_var.hpp>
#include <coma/co_lift.hpp>
#include <test_util.hpp>

static_assert(!std::is_copy_constructible<coma::async_cond_var<>>::value, "");
static_assert(!std::is_move_constructible<coma::async_cond_var<>>::value, "");
static_assert(!std::is_copy_assignable<coma::async_cond_var<>>::value, "");
static_assert(!std::is_move_assignable<coma::async_cond_var<>>::value, "");

TEST_CASE("async_cond_var ctor", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};
}

TEST_CASE("async_cond_var wait", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};

    int done = 0;
    cv.async_wait([&](boost::system::error_code ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.poll();
    CHECK(done == 0);

    boost::asio::post(ctx, [&] {
        cv.notify_one();
    });
    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_cond_var wait many fifo", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};

    int done = 0;
    cv.async_wait([&](boost::system::error_code ec) {
        CHECK(!ec);
        CHECK(done == 0);
        done = 1;
    });
    cv.async_wait([&](boost::system::error_code ec) {
        CHECK(!ec);
        CHECK(done == 1);
        done = 2;
    });
    boost::asio::post(ctx, [&] {
        cv.notify_all();
    });

    ctx.run();
    CHECK(done == 2);
}

TEST_CASE("async_cond_var wait many fifo one", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};

    int done = 0;
    cv.async_wait([&](boost::system::error_code ec) {
        CHECK(!ec);
        CHECK(done == 0);
        done = 1;
    });
    cv.async_wait([&](boost::system::error_code ec) {
        CHECK(!ec);
        CHECK(done == 1);
        done = 2;
    });
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
    coma::async_cond_var<> cv{ctx.get_executor()};

    int done = 0;
    cv.async_wait(coma::make_logged_fn([&] { return done == 2; }),
        [&](boost::system::error_code ec) {
            CHECK(!ec);
            CHECK(done == 2);
            done = 3;
        });
    cv.async_wait(coma::make_logged_fn([&] { return done == 1; }),
        [&](boost::system::error_code ec) {
            CHECK(!ec);
            CHECK(done == 1);
            done = 2;
            cv.notify_all();
        });

    cv.notify_all();
    ctx.poll();
    CHECK(done == 0);

    done = 1;
    cv.notify_all();

    ctx.run();
    CHECK(done == 3);
}

#if defined(COMA_COROUTINES) && defined(COMA_ENABLE_COROUTINE_TESTS)

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using cond_var = boost::asio::use_awaitable_t<>::as_default_on_t<coma::async_cond_var<>>;

TEST_CASE("async_cond_var coro wait", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    cond_var cv{ctx.get_executor()};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await cv.async_wait();
        ++done;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        cv.notify_one();
        co_return;
    }, boost::asio::detached);

    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_cond_var coro wait many fifo", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};

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

TEST_CASE("async_cond_var coro wait many fifo one", "[async_cond_var]")
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};

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

TEST_CASE("async_cond_var coro wait many pred", "[async_cond_var]")
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
