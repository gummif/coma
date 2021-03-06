#include <coma/async_cond_var.hpp>
#include <test_util.hpp>

#ifdef COMA_HAS_DEFAULT_IO_EXECUTOR
using async_cond_var = coma::async_cond_var<>;
#else
using async_cond_var = coma::async_cond_var<boost::asio::io_context::executor_type>;
#endif

static_assert(!std::is_copy_constructible<async_cond_var>::value, "");
static_assert(!std::is_move_constructible<async_cond_var>::value, "");
static_assert(!std::is_copy_assignable<async_cond_var>::value, "");
static_assert(!std::is_move_assignable<async_cond_var>::value, "");

TEST_CASE("async_cond_var ctor", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
}

#ifdef COMA_HAS_AS_DEFAULT_ON
TEST_CASE("async_cond_var as default on detached", "[async_cond_var]")
{
    using cv_d = boost::asio::detached_t::as_default_on_t<coma::async_cond_var<boost::asio::io_context::executor_type>>;
	boost::asio::io_context ctx;
	cv_d cv{ctx.get_executor()};
    cv.async_wait();
}
#endif

TEST_CASE("async_cond_var wait", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(!ec);
		++done;
	});
	ctx.poll();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] { cv.notify_one(); });
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var wait race condition", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int val = 0;
	int val_in_pred = 0;

	boost::asio::post(ctx, [&] {
		boost::asio::post(ctx, [&] { ++val; });
		CHECK(val == 0);
		// verify that initially true pred will also be
		// true inside handler
		cv.async_wait([&] {
				val_in_pred = val;
				return true;
			},
			[&](boost::system::error_code ec) {
				CHECK(!ec);
				CHECK(val == val_in_pred);
				++val;
			});
	});
	ctx.run();
	CHECK(val == 2);
}

TEST_CASE("async_cond_var wait many fifo", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

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
	boost::asio::post(ctx, [&] { cv.notify_all(); });

	ctx.run();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var wait many fifo one", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

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
	async_cond_var cv{ctx.get_executor()};

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

	CHECK(coma::run_would_block(ctx));

	done = 1;
	cv.notify_all();

	ctx.run();
	CHECK(done == 3);
}

#if defined(COMA_COROUTINES) && defined(COMA_ENABLE_COROUTINE_TESTS)

using boost::asio::awaitable;
using boost::asio::use_awaitable;

#ifdef COMA_HAS_AS_DEFAULT_ON
TEST_CASE("async_cond_var coro as default on", "[async_cond_var]")
{
	using cond_var_coro = boost::asio::use_awaitable_t<>::as_default_on_t<coma::async_cond_var<boost::asio::io_context::executor_type>>;

	boost::asio::io_context ctx;
	cond_var_coro cv{ctx.get_executor()};

	boost::asio::co_spawn(
		ctx,
		[&]() -> awaitable<void> {
			co_await cv.async_wait(coma::logged_fn([&] { return true; }));
		},
		boost::asio::detached);
	ctx.run();
}
#endif

TEST_CASE("async_cond_var coro wait many pred", "[async_cond_var]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	boost::asio::co_spawn(
		ctx,
		[&]() -> awaitable<void> {
			TEST_LOG("before await");
			co_await cv.async_wait(coma::logged_fn([&] { return done == 2; }), use_awaitable);
			TEST_LOG("after await");
			CHECK(done == 2);
			done = 3;
		},
		boost::asio::detached);
	boost::asio::co_spawn(
		ctx,
		[&]() -> awaitable<void> {
			TEST_LOG("before await");
			co_await cv.async_wait(coma::logged_fn([&] { return done == 1; }), use_awaitable);
			TEST_LOG("after await");
			CHECK(done == 1);
			done = 2;
			cv.notify_all();
		},
		boost::asio::detached);

	boost::asio::post(ctx, [&] { cv.notify_all(); });
	ctx.poll();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] {
		done = 1;
		cv.notify_all();
	});

	ctx.run();
	CHECK(done == 3);
}

#endif
