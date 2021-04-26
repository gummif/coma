# coma
[![ubuntu linux](https://github.com/gummif/coma/actions/workflows/linux.yml/badge.svg)](https://github.com/gummif/coma/actions?query=workflow%3ALinux)
[![windows](https://github.com/gummif/coma/actions/workflows/windows.yml/badge.svg)](https://github.com/gummif/coma/actions?query=workflow%3AWindows)


- [Introduction](#introduction)
- [Overview and comparison](#overview-and-comparison)
- [Examples](#examples)
- [Synopsis](#synopsis)
- [Nomenclature](#nomenclature)

# Introduction

Coma is a C++11 header-only library providing asynchronous concurrency primatives based on the asynchronous model of Boost.ASIO. Utilities include RAII guards for semaphores, async semaphores, async condition variables and async thread-safe wrappers and handles.

Coma depends on Boost.ASIO (executors, timers and utilities) and Boost.Beast (initiating function and composed operation utilities only) and familiarity with executors and completion handlers is a prerequisite. Minimum supported Boost version is 1.72. An exception is made for the semaphore guards, which only depend on the standard library and can be used without bringing in Boost.

To integrate it into you project you can add `include` to your include directories, use `add_subdirectory(path/to/coma)` or download it automatically using [FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) from your CMake project, or use the Conan package manager (WIP).

The library provides:

* `coma::async_semaphore` lightweight async semaphore, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks.
* `coma::async_cond_var` lightweight async condition variable, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks and without spurious wakening.
* `coma::acquire_guard` equivalent to `std::lock_guard` for semaphores using acquire/release instead of lock/unlock.
* `coma::unique_acquire_guard` equivalent to `std::unique_lock` for semaphores using acquire/release instead of lock/unlock.

And currently experimental or work in progress:
* `coma::async_semaphore_timed` with timed functions.
* `coma::async_cond_var_timed` with timed functions, cancellation.
* `coma::async_synchronized` thread-safe async wrapper of values through a strand (similar to proposed `std::synchronized_value`).
* `coma::co_lift` a higher-order function to lift regular functions `R()` into `awaitable<R>()`.

Coma is tested with:
* GCC 10.2 (C++20, address sanitizer, coroutines, Boost 1.76)
* GCC 10.2 (C++11, address sanitizer, Boost 1.76)
* GCC 10.2 (C++11, address sanitizer, Boost 1.72)
* Clang 11.0 (C++20, address sanitizer, Boost 1.76)
* MSVC 16.9 (C++20, Boost 1.76).
* MSVC 16.9 (C++17, Boost 1.76).

## Overview and comparison

There are a few variant of semaphores and condition variables where there is a tradeoff between the guarantees the types make and completeness of the API versus performance.

| Type                      | Async | Thread-safe | Timeout |
|---------------------------|-------|-------|------|
| `std::counting_semaphore` | No | **Yes** | **Yes** |
| `coma::async_semaphore` | **Yes** | No | No |
| `coma::async_semaphore_timed` | **Yes** | No | **Yes** |
| `coma::async_semaphore_s`? | **Yes** | **Yes** | No |
| `coma::async_semaphore_timed_s` | **Yes** | **Yes** | **Yes** |

| Type                      | Async | Thread-safe | Timeout | Cancellation | SW\* |
|---------------------------|-------|-------|------|------|------|
| `std::conndition_variable` | No | **Yes** | **Yes** | No | **Yes** |
| `std::conndition_variable_any` | No | **Yes** | **Yes** | **Yes** | **Yes** |
| `coma::async_cond_var` | **Yes** | No | No | No | No |
| `coma::async_cond_var_timed` | **Yes** | No | **Yes** | **Yes** | **Yes** |

\* SW = spurious wakeup

## Examples

Using async semaphore as a lightweight async latch between two threads. This example spawns a new thread to execute some heavy task without blocking the current executor/execution context. Using `async_acquire_n` to synchronize the completion of multiple task is left as an excercise. The examples use coroutines with `awaitable` for simplicity, but the library supports any valid completion token (such as callbacks).

```c++
// execute f on a new thread without blocking current executor
template<class F, class R = decltype(f())>
auto co_spawn_thread(F f) -> net::awaitable<R>
{
    coma::async_semaphore_s sem{co_await net::this_coro::executor, 0};
    R ret;
    std::exception_ptr e;
    std::thread([&]() noexcept {
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
    }).detach();
    
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
    using executor_type = typename coma::async_cond_var_timed<>::executor_type;
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

Async condition variable with timeout and cancellation support via `std::stop_token` and `std::stop_source`.
```c++
coma::async_timed_cond_var cv;
bool set;
int answer;

net::awaitable<int> f(std::stop_token st)
{
    if (co_await cv.async_wait_for(1s, [&] { return set; }, st))
        co_return answer;
    else if (st.stop_requested())
        throw std::runtime_error("cancelled");
    else
        throw std::runtime_error("timeout");
}
```

## Synopsis

In header `<coma/async_semaphore.hpp>`
```c++
template<class Executor COMA_SET_DEFAULT_IO_EXECUTOR>
class async_semaphore;
```

In header `<coma/async_cond_var.hpp>`
```c++
template<class Executor COMA_SET_DEFAULT_IO_EXECUTOR>
class async_cond_var;
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
