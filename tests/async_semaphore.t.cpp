#include <coma/async_semaphore.hpp>
#include <coma/co_lift.hpp>
#include <test_util.hpp>

static_assert(!std::is_copy_constructible<coma::async_semaphore<>>::value, "");
static_assert(!std::is_move_constructible<coma::async_semaphore<>>::value, "");
static_assert(!std::is_copy_assignable<coma::async_semaphore<>>::value, "");
static_assert(!std::is_move_assignable<coma::async_semaphore<>>::value, "");

TEST_CASE("async_semaphore ctor", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 1};

    REQUIRE(sem.try_acquire());
    REQUIRE(!sem.try_acquire());
    sem.release();
    REQUIRE(sem.try_acquire());
    REQUIRE(!sem.try_acquire());
    sem.release(2);
    REQUIRE(sem.try_acquire());
    REQUIRE(sem.try_acquire());
    REQUIRE(!sem.try_acquire());

    sem.release();
    {
        REQUIRE(sem.try_acquire());
        REQUIRE(!sem.try_acquire());
        coma::acquire_guard g{sem, coma::adapt_acquire};
        REQUIRE(!sem.try_acquire());
    }
    REQUIRE(sem.try_acquire());
    sem.release();
    {
        coma::unique_acquire_guard g{sem, coma::try_to_acquire};
        CHECK(g);
        REQUIRE(!sem.try_acquire());
    }
    REQUIRE(sem.try_acquire());
}

TEST_CASE("async_semaphore async_acquire waiting", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.poll();
    CHECK(done == 0);
    boost::asio::post(ctx, [&] {
        sem.release();
    });

    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_semaphore async_acquire immediate", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 1};

    int done = 0;
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_semaphore async_acquire many", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.poll();
    CHECK(done == 0);

    boost::asio::post(ctx, [&] {
        sem.release();
    });
    ctx.poll();
    CHECK(done == 1);

    boost::asio::post(ctx, [&] {
        sem.release();
    });
    ctx.run();
    CHECK(done == 2);
}

TEST_CASE("async_semaphore async_acquire many release 2", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    sem.async_acquire([&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.poll();
    CHECK(done == 0);

    boost::asio::post(ctx, [&] {
        sem.release(2);
    });
    ctx.run();
    CHECK(done == 2);
}

TEST_CASE("async_semaphore async_acquire_n immediate", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 2};

    int done = 0;
    sem.async_acquire_n(2, [&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_semaphore async_acquire_n", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    sem.async_acquire_n(2, [&](auto ec) {
        CHECK(!ec);
        ++done;
    });
    ctx.poll();
    CHECK(done == 0);

    boost::asio::post(ctx, [&] {
        sem.release();
    });
    ctx.poll();
    CHECK(done == 0);

    boost::asio::post(ctx, [&] {
        sem.release();
    });
    ctx.run();
    CHECK(done == 1);
}

#if defined(COMA_ENABLE_COROUTINE_TESTS)

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using semaphore = boost::asio::use_awaitable_t<>::as_default_on_t<coma::async_semaphore<>>;

TEST_CASE("async_semaphore coro async_acquire waiting", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);
    ctx.poll();
    CHECK(done == 0);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        sem.release();
        co_return;
    }, boost::asio::detached);

    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_semaphore coro async_acquire immediate", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    semaphore sem{ctx.get_executor(), 1};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);
    ctx.run();
    CHECK(done == 1);
}

TEST_CASE("async_semaphore coro async_acquire many", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);

    ctx.poll();
    CHECK(done == 0);

    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        sem.release();
        co_return;
    }, boost::asio::detached);

    ctx.poll();
    CHECK(done == 1);

    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        sem.release();
        co_return;
    }, boost::asio::detached);

    ctx.run();
    CHECK(done == 2); 
}

TEST_CASE("async_semaphore coro async_acquire many release 2", "[async_semaphore]")
{
    boost::asio::io_context ctx;
    semaphore sem{ctx.get_executor(), 0};

    int done = 0;
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);
    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        co_await sem.async_acquire();
        ++done;
    }, boost::asio::detached);

    ctx.poll();
    CHECK(done == 0);

    boost::asio::co_spawn(ctx, [&]() -> awaitable<void> {
        sem.release(2);
        co_return;
    }, boost::asio::detached);

    ctx.run();
    CHECK(done == 2);
}

#endif // defined(COMA_ENABLE_COROUTINE_TESTS)
