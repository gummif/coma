#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <catch2/catch.hpp>
#include <iostream>
#include <vector>

#define DO_TEST_LOG

#ifdef DO_TEST_LOG 
#define TEST_LOG(x) std::cerr << x << std::endl
#else
#define TEST_LOG(x)
#endif

namespace coma {

struct logged
{
    logged() noexcept { TEST_LOG("logged ctor"); }
    ~logged() noexcept { TEST_LOG("logged dtor "); }
    logged(logged&&) noexcept { TEST_LOG("logged move ctor"); }
    logged& operator=(logged&&) noexcept { TEST_LOG("logged move assign"); return *this; }
    logged(const logged&) = delete;
    logged& operator=(const logged&) = delete;
};

struct logged_copyable
{
    logged_copyable() noexcept { TEST_LOG("logged_copyable ctor"); }
    ~logged_copyable() noexcept { TEST_LOG("logged_copyable dtor "); }
    logged_copyable(logged_copyable&&) noexcept { TEST_LOG("logged_copyable move ctor"); }
    logged_copyable& operator=(logged_copyable&&) noexcept { TEST_LOG("logged_copyable move assign"); return *this; }
    logged_copyable(const logged_copyable&)  { TEST_LOG("logged_copyable copy ctor"); }
    logged_copyable& operator=(const logged_copyable&)  { TEST_LOG("logged_copyable copy assign"); return *this; }
};

template<class F>
struct logged_fn
{
    F f;
    logged_fn(F g) : f{std::move(g)} { TEST_LOG("logged_fn ctor"); }
    ~logged_fn() { TEST_LOG("logged_fn dtor "); }
    logged_fn(logged_fn&& other) noexcept : f{std::move(other.f)} { TEST_LOG("logged_fn move ctor"); }
    logged_fn& operator=(logged_fn&& other) noexcept { TEST_LOG("logged_fn move assign"); f = std::move(other.f); return *this; }
    logged_fn(const logged_fn&) = delete;
    logged_fn& operator=(const logged_fn&) = delete;
    decltype(auto) operator()()
    {
        TEST_LOG("logged_fn op");
        return f();
    }
    decltype(auto) operator()() const
    {
        TEST_LOG("logged_fn op");
        return f();
    }
};

template<class F>
struct finally
{
	F f;
	finally(auto g) : f{std::move(g)} { TEST_LOG("finally ctor"); }
	~finally() { TEST_LOG("finally ctor"); if (f) f(); }
};


}
