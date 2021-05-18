# coma
[![ubuntu linux](https://github.com/gummif/coma/actions/workflows/linux.yml/badge.svg)](https://github.com/gummif/coma/actions?query=workflow%3ALinux)
[![windows](https://github.com/gummif/coma/actions/workflows/windows.yml/badge.svg)](https://github.com/gummif/coma/actions?query=workflow%3AWindows)


- [Introduction](#introduction)
- [Overview](#overview)
- [Design](#design)
- [Examples](#examples)
- [Gotchas](#gotchas)
- [Synopsis](#synopsis)
- [Nomenclature](#nomenclature)

# Introduction

Coma is a C++11 header-only library providing asynchronous concurrency primatives, based on the asynchronous model of [Boost.ASIO](https://www.boost.org/doc/libs/1_76_0/doc/html/boost_asio.html) (and proposed [standard executors](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r14.html)). Utilities include RAII guards for semaphores, async semaphores and async condition variables.

Coma depends on Boost.ASIO (executors, timers and utilities) and Boost.Beast (initiating function and composed operation utilities) and familiarity with executors and completion handlers/tokens is a prerequisite. Minimum supported Boost version is 1.72. An exception is made for the semaphore guards, which only depend on the standard library and can be used without bringing in Boost.

To integrate it into you project you can add `include` to your include directories, use `add_subdirectory(path/to/coma)` or download it automatically using [FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) from your CMake project, or use the Conan package manager (WIP).

The library provides:

* `coma::async_semaphore` lightweight async semaphore, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks.
* `coma::async_cond_var` lightweight async condition variable, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks and without spurious wakening.
* `coma::async_cond_var_timed` lightweight async condition variable, _not_ thread-safe, with support for timed waits and cancellation. With FIFO ordering of waiting tasks. May experience spurious wakening.
* `coma::acquire_guard` equivalent to `std::lock_guard` for semaphores using acquire/release instead of lock/unlock.
* `coma::unique_acquire_guard` equivalent to `std::unique_lock` for semaphores using acquire/release instead of lock/unlock.

Work in progress:
* `coma::async_semaphore_timed` with timed functions and cancellation.
* `coma::async_semaphore_timed_s` synchronized variant.
* `coma::async_synchronized` thread-safe async wrapper of values through a strand (similar to proposed `std::synchronized_value`).

Coma is tested with:
* GCC 10.2 (C++20, address sanitizer, coroutines, Boost 1.76)
* GCC 10.2 (C++11, address sanitizer, Boost 1.76)
* GCC 10.2 (C++11, address sanitizer, Boost 1.72)
* Clang 11.0 (C++20, address sanitizer, Boost 1.76)
* MSVC 16.9 (C++20, Boost 1.76).
* MSVC 16.9 (C++17, Boost 1.76).

## Overview

There are a few variant of semaphores and condition variables where there is a tradeoff between the guarantees the types make and completeness of the API versus performance.

| Semaphore                 | Async | Thread-safe | Timeout |
|---------------------------|-------|-------|------|
| `std::counting_semaphore` | No | **Yes** | **Yes** |
| `coma::async_semaphore` | **Yes** | No | No |
| `coma::async_semaphore_timed` (WIP) | **Yes** | No | **Yes** |
| `coma::async_semaphore_timed_s` (WIP) | **Yes** | **Yes** | **Yes** |

| Condition variable        | Async | Thread-safe | Timeout | Cancellation | SW\* |
|---------------------------|-------|-------|------|------|------|
| `std::conndition_variable` | No | **Yes** | **Yes** | No | **Yes** |
| `std::conndition_variable_any` | No | **Yes** | **Yes** | **Yes** | **Yes** |
| `coma::async_cond_var` | **Yes** | No | No | No | No |
| `coma::async_cond_var_timed` | **Yes** | No | **Yes** | **Yes** | **Yes** |

\* SW = spurious wakeup

## Examples
Most of the examples use coroutines with `awaitable` for simplicity, but the library supports any valid completion token (such as callbacks). The examples will use the alias `net` for `boost::asio`.

Hello world:
```c++
int main()
{
    net::io_context ctx;
    coma::async_cond_var cv{ctx.get_executor()};

    bool done = false;
    cv.async_wait([&] { return done; },
        [&](boost::system::error_code ec) {
            assert(!ec);
            puts("Hello world!");
        });
    net::post(ctx, [&] {
        done = true;
        cv.notify_one();
    });
    ctx.run();
}
```

Limiting number of resources in use with a semaphore:
```c++
net::awaitable<void>
handle_connection(tcp_socket socket,
                  coma::async_acquire_guard guard);
net::awaitable<void> listen(tcp_listener listener,
                            coma::async_semaphore_timed_s sem)
{
    while (true)
    {
        co_await sem.async_acquire(net::use_awaitable);
        coma::async_acquire_guard guard{sem, coma::adapt_acquire};

        auto socket = listener.async_listen(net::use_awaitable);
        // note: unstructured concurrency, that's why
        // we need a synchronized semaphore and
        // async_acquire_guard for this to be safe
        net::co_spawn(co_await net::this_coro::executor,
            handle_connection(std::move(socket), std::move(g)),
            net::detached);
    }
}
```

Using async semaphore as a lightweight async latch between two threads. This example spawns a new thread to execute some heavy task without blocking the current executor/execution context. Using `async_acquire_n` to synchronize the completion of multiple task is left as an excercise. 

```c++
// execute f on in new thread without blocking current executor
template<class F, class R = decltype(f())>
auto co_spawn_thread(F f) -> net::awaitable<R>
{
    coma::async_semaphore_s sem{co_await net::this_coro::executor, 0};
    R ret;
    std::exception_ptr e;
    // if we use jthread then the shared state can
    // live on the stack, otherwise we would need to
    // store it in a shared_ptr
    std::jthread t([&]() noexcept {
        // release at scope exit
        coma::acquire_guard g{sem, coma::adapt_acquire};
        try
        {
            ret = f();
        }
        catch (...)
        {
            e = std::current_exception();
        }
    });
    
    // non-blocking wait for thread to finish
    co_await sem.async_acquire(net::use_awaitable);
    if (e)
        std::rethrow_exception(e);
    co_return std::move(ret);
}

int blocking_get_answer()
{
    //simulate CPU bound or blocking operation, may throw
    std::this_thread::sleep_for(1s);
    return 42;
}

net::awaitable<int> async_get_answer()
{
    return co_spawn_thread(&blocking_get_answer);
}
```

An async queue is almost trivial to write with an async condition variable and coroutines.
```c++
template<class T>
class async_queue
{
public:
    using executor_type = typename coma::async_cond_var<>::executor_type;
    explicit async_queue(const executor_type& ex) : cv{ex}
    {}
    net::awaitable<T> async_pop()
    {
        co_await cv.async_wait([&] {
            return !q.empty();
        }, net::use_awaitable);
        auto item = std::move(q.front());
        q.pop_front();
        co_return std::move(item);
    }
    void push(T item)
    {
        q.push_back(std::move(item));
        cv.notify_one();
    }
private:
    coma::async_cond_var<> cv;
    std::queue<T> q;
};
```

We now make a thread-safe async queue (a go channel if you will) based on the above queue and `coma::async_synchronized` (WIP):
```c++
template<class T>
class async_queue_s
{
public:
    using executor_type = typename async_queue<T>::executor_type;
    explicit async_queue_s(const executor_type& ex) : sq{ex, std::in_place, ex}
    {}
    net::awaitable<T> async_pop()
    {
        return sq.invoke([](auto& q) {
            return q.async_pop();
        }, net::use_awaitable);
        // NOTE this is the non-coroutine "awaitable backwarding" variant of
        // co_return co_await sq.invoke([](auto& q) -> net::awaitable<T> {
        //   co_return co_await q.async_pop(); });
    }
    net::awaitable<void> async_push(T item)
    {
        return sq.invoke([item = std::move(item)](auto& q) mutable
        {
            q.push(std::move(item));
        }, net::use_awaitable);
    }
private:
    coma::async_synchronized<async_queue<T>> sq;
};
```

## Design

The non-thread-safe/unsynchronized types are designed efficient use in single threaded (or externally synchronized context e.g. via a strand). If you are unsure which variant to use in your program then the synchronized variants (`*_s`) are a good choise (since they are both thread-safe and atomically reference counted). Also note that the semaphore guards can be dangerous if used ina a non-structured concurrency manner (see Gotchas section below). Be espepecially careful when using `detached` or `execute`/`post` without reference counting.

## Gotchas

There are many ways to shoot yourself in the foot with the unsynchronized variants:
```c++
void BAD()
{
    net::io_context ctx;
    coma::async_semaphore sem{ctx.get_executor(), 1};
    net::co_spawn(ctx, [&]() -> net::awaitable<void>
    {
        co_await sem.async_acquire(net::use_awaitabke);
        coma::acquire_guard g{sem, coma::adapt_lock};
        co_await some_async_op();
    }, net::detached);
    // ... setup to stop ctx on interrupt
    ctx.run();
    // sem may be destructed before the destructor
    // for the spawned task runs resulting in
    // use after free in ~acquire_guard()
}

void OK()
{
    net::io_context ctx;
    net::co_spawn(ctx, []() -> net::awaitable<void>
    {
        // OK structured concurrency within this task
        coma::async_semaphore sem{ctx.get_executor(), 1};
        co_await sem.async_acquire(net::use_awaitabke);
        coma::acquire_guard g{sem, coma::adapt_lock};
        co_await some_async_op();
    }, net::detached);
    // ... setup to stop ctx on interrupt
    ctx.run();
}
```

## Synopsis

In header `<coma/async_semaphore.hpp>`
```c++
template<class Executor>
class async_semaphore;
```

In header `<coma/async_cond_var.hpp>`
```c++
template<class Executor>
class async_cond_var;
```

In header `<coma/async_cond_var_timed.hpp>`
```c++
template<class Executor>
class async_cond_var_timed;
```

In header `<coma/semaphore_guards.hpp>`
```c++
template<class Semaphore>
class acquire_guard;

template<class Semaphore>
class unique_acquire_guard;
```

## Nomenclature

We can try to classify functions that do multi-thread or multi-task synchronization into:

* *Asynchronous*: Possibly deferred function continuation. Can be
    * waiting (`awaitable<void> async_lock()`)
    * non-waiting  (`awaitable<bool> async_try_lock()`)
* *Synchronous*: Inline function completion. Can be
    * blocking (`void lock()`) or 
    * non-blocking (`bool try_lock()`).

Async vs sync in general says nothing of the thread-safety of a function. Functions in this library should be considered non-thread-safe unless specified otherwise. Classes with a `_s` suffix provide a thread-safe API.
