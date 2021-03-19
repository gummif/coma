#include <coma/async_semaphore.hpp>

#include <iostream>
#include <functional>
#include <thread>

#include <asio/steady_timer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/strand.hpp>
#include <asio/experimental/as_single.hpp>

struct finally
{
	std::function<void()> f;
	finally(auto g) : f{g} { puts("finally ctor"); }
	~finally() { if (f) f(); }
};

int main()
{
	asio::io_context ctx{1};

    asio::strand<asio::io_context::executor_type> strand{ctx.get_executor()};
    coma::async_semaphore sem{ctx.get_executor(), 0};
                    std::cout << "sem address = " << static_cast<void*>(&sem) << std::endl;

	asio::steady_timer t{ctx};
	t.expires_after(std::chrono::seconds(1));
	
	asio::co_spawn(ctx.get_executor(), [&]() -> asio::awaitable<void>
	{
        finally fin{[] { puts("co finally"); }};
        puts("co about to acquire");

        co_await sem.async_acquire();
        coma::async_scoped_acquire_guard g{sem, coma::adapt_acquire};
        
	}, asio::detached);

    asio::co_spawn(ctx.get_executor(), [&]() -> asio::awaitable<void>
	{
        finally fin{[] { puts("co2 finally"); }};
        // NOTE forgetting to await, no warning
        puts("co2 about to sleep");
        co_await t.async_wait(asio::use_awaitable);
        puts("co2 releasing");
        co_await sem.async_release();
        //sem.release();
        puts("co2 beofore strand post");
        auto ex = co_await asio::this_coro::executor;
        co_await asio::co_spawn(strand, [&]() -> asio::awaitable<void>
        {
            auto exs = co_await asio::this_coro::executor;
            puts(ex == exs ? "same as outside post" : "diferent in post");
        }, asio::use_awaitable);
        auto ex2 = co_await asio::this_coro::executor;
        puts(ex == ex2 ? "same as before post" : "diferent after post");
	}, asio::detached);

    asio::co_spawn(ctx.get_executor(), [&]() -> asio::awaitable<void>
	{
        auto ex = co_await asio::this_coro::executor;
        co_await asio::post(strand, asio::use_awaitable);
        puts(strand.running_in_this_thread()?"yes":"no"); // yes
        auto ex2 = co_await asio::this_coro::executor;
        puts(ex == ex2 ? "same as before post AW" : "diferent after post AW"); // but same
        co_await t.async_wait(asio::use_awaitable);
        puts(strand.running_in_this_thread()?"yes":"no"); // no
	}, asio::detached);

	puts("-run1");
	ctx.run_one();


	puts("-run2");
	ctx.run_one();

	std::thread([&] { puts("-run1 on another thread"); ctx.run_one(); }).join();

	puts("-run3");
	ctx.run_one();

	puts("-run4");
	ctx.run_one();

	puts("-run5");
	ctx.run_one();

	puts("-run6");
	ctx.run_one();

	puts("-run7");
	ctx.run_one();

	puts("done");

	return 0;
}
