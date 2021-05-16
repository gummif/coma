#include <coma/experimental/async_cond_var_timed.hpp>
#include <test_util.hpp>
#include <boost/asio/strand.hpp>

#ifdef COMA_HAS_DEFAULT_IO_EXECUTOR
using async_cond_var = coma::async_cond_var_timed<>;
#else
using async_cond_var = coma::async_cond_var_timed<boost::asio::io_context::executor_type>;
#endif

static_assert(!std::is_copy_constructible<async_cond_var>::value, "");
static_assert(!std::is_move_constructible<async_cond_var>::value, "");
static_assert(!std::is_copy_assignable<async_cond_var>::value, "");
static_assert(!std::is_move_assignable<async_cond_var>::value, "");

TEST_CASE("async_cond_var_timed ctor", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
}

TEST_CASE("async_cond_var_timed wait no run", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	cv.async_wait([](boost::system::error_code){});
}

TEST_CASE("async_cond_var_timed wait poll", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	cv.async_wait([](boost::system::error_code){});
	ctx.poll();
}

TEST_CASE("async_cond_var_timed wait ctx restart", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	cv.async_wait([](boost::system::error_code){});
	ctx.poll();
	ctx.restart();
	CHECK(coma::run_would_block(ctx));
	ctx.restart();
	ctx.stop();
	ctx.restart();
	ctx.poll();
}

#ifdef COMA_HAS_AS_DEFAULT_ON
TEST_CASE("async_cond_var_timed as default on detached", "[async_cond_var_timed]")
{
    using cv_d = boost::asio::detached_t::as_default_on_t<coma::async_cond_var_timed<boost::asio::io_context::executor_type>>;
	boost::asio::io_context ctx;
	cv_d cv{ctx.get_executor()};
    cv.async_wait();
}
#endif

TEST_CASE("async_cond_var_timed wait", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(!ec);
		++done;
	});
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] { cv.notify_one(); });
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait_for timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	cv.async_wait_for(std::chrono::milliseconds{1},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::timeout);
			++done;
		});
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait_for pred timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	bool trigger = false;
	int done = 0;
	cv.async_wait_for(std::chrono::milliseconds{1},
		[&] { return trigger; },
		[&](boost::system::error_code ec, bool b) {
			CHECK(!ec);
			CHECK(!b);
			++done;
		});
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait_for no timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	cv.async_wait_for(std::chrono::seconds{100},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] { cv.notify_one(); });
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait_for pred no timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	bool trigger = false;
	int done = 0;
	cv.async_wait_for(std::chrono::seconds{100},
		[&] { return trigger; },
		[&](boost::system::error_code ec, bool b) {
			CHECK(!ec);
			CHECK(b);
			++done;
		});
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] {
		trigger = true;
		cv.notify_one();
	});
	ctx.run();
	CHECK(done == 1);
}


TEST_CASE("async_cond_var_timed wait_for many timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	cv.async_wait_for(std::chrono::seconds{100},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	// this will trigger the previous wait
	cv.async_wait_for(std::chrono::milliseconds{1},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::timeout);
			++done;
		});
	ctx.run();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var_timed wait_for many timeout 2", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	
	int done = 0;
	cv.async_wait_for(std::chrono::milliseconds{1},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::timeout);
			++done;
		});
	// this will trigger the previous wait
	cv.async_wait_for(std::chrono::seconds{100},
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	ctx.run();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var_timed wait_until many timeout", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds{100};
	int done = 0;
	cv.async_wait_until(end,
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	cv.async_wait_until(end,
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);
	cv.notify_one();
	ctx.poll();
	ctx.restart();
	CHECK(done == 1);
	cv.notify_one();
	ctx.run();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var_timed wait_until many timeout 2", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};
	
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds{100};
	int done = 0;
	cv.async_wait_until(end,
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	cv.async_wait_until(end,
		[&](boost::system::error_code ec, coma::cv_status s) {
			CHECK(!ec);
			CHECK(s == coma::cv_status::no_timeout);
			++done;
		});
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);
	cv.notify_all();
	ctx.run();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var_timed wait race condition", "[async_cond_var_timed]")
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


TEST_CASE("async_cond_var_timed wait already stopped", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	cv.stop();
	int done = 0;

	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(ec == boost::asio::error::operation_aborted);
		done = 1;
	});
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait pred already stopped", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	cv.stop();
	int done = 0;

	cv.async_wait([&] { return false; }, [&](boost::system::error_code ec) {
		CHECK(ec == boost::asio::error::operation_aborted);
		done = 1;
	});
	ctx.run();
	CHECK(done == 1);
}

TEST_CASE("async_cond_var_timed wait stop", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	async_cond_var cv{ctx.get_executor()};

	int done = 0;

	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(ec == boost::asio::error::operation_aborted);
		CHECK(done == 0);
		done = 1;
	});
	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(ec == boost::asio::error::operation_aborted);
		CHECK(done == 1);
		done = 2;
	});

	CHECK(coma::run_would_block(ctx));
	ctx.restart();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] {
		CHECK(!cv.stopped());
		cv.stop();
		CHECK(cv.stopped());
	});

	ctx.run();
	ctx.restart();
	CHECK(done == 2);

	CHECK(cv.stopped());
	cv.async_wait([&](boost::system::error_code ec) {
		CHECK(ec == boost::asio::error::operation_aborted);
		done = 3;
	});

	ctx.run();
	ctx.restart();
	CHECK(done == 3);

	boost::asio::post(ctx, [&] {
		cv.restart();
		CHECK(!cv.stopped());
		cv.async_wait([&](boost::system::error_code ec) {
			CHECK(!ec);
			done = 4;
		});
		boost::asio::post(ctx, [&] {
			cv.notify_one();
		});
	});

	ctx.run();
	CHECK(done == 4);
}

TEST_CASE("async_cond_var_timed wait many fifo", "[async_cond_var_timed]")
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

TEST_CASE("async_cond_var_timed wait many fifo one", "[async_cond_var_timed]")
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
	ctx.restart();
	CHECK(done == 0);
	cv.notify_one();
	ctx.poll();
	ctx.restart();
	CHECK(done == 1);
	cv.notify_one();
	ctx.poll();
	CHECK(done == 2);
}

TEST_CASE("async_cond_var_timed wait many pred", "[async_cond_var_timed]")
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
	ctx.restart();
	CHECK(done == 0);

	done = 1;
	cv.notify_all();

	ctx.run();
	CHECK(done == 3);
}

TEST_CASE("async_cond_var_timed pred strand", "[async_cond_var_timed]")
{
	boost::asio::io_context ctx;
	boost::asio::strand<typename boost::asio::io_context::executor_type> strand{ctx.get_executor()};
	async_cond_var cv{ctx.get_executor()};

	int done = 0;
	bool p = false;
	cv.async_wait([&] { 
			CHECK(strand.running_in_this_thread());
			return p;
		}, boost::asio::bind_executor(strand,
		[&](boost::system::error_code ec) {
			CHECK(strand.running_in_this_thread());
			CHECK(!ec);
			CHECK(done == 0);
			done = 1;
		}));
	ctx.poll();
	ctx.restart();
	CHECK(done == 0);
	p = true;
	cv.notify_one();
	ctx.run();
	CHECK(done == 1);
}

#if defined(COMA_COROUTINES) && defined(COMA_ENABLE_COROUTINE_TESTS)

using boost::asio::awaitable;
using boost::asio::use_awaitable;

#ifdef COMA_HAS_AS_DEFAULT_ON
TEST_CASE("async_cond_var_timed coro as default on", "[async_cond_var_timed]")
{
	using cond_var_coro = boost::asio::use_awaitable_t<>::as_default_on_t<coma::async_cond_var_timed<boost::asio::io_context::executor_type>>;

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

TEST_CASE("async_cond_var_timed coro wait many pred", "[async_cond_var_timed]")
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
	ctx.restart();
	CHECK(done == 0);

	boost::asio::post(ctx, [&] {
		done = 1;
		cv.notify_all();
	});

	ctx.run();
	CHECK(done == 3);
}

#endif
