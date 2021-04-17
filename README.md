# coma
![ubuntu linux](https://github.com/gummif/coma/actions/workflows/linux.yml/badge.svg)


Coma is a C++11 header-only library providing asynchronous concurrency primatives based on the asynchronous model of Boost.ASIO. Utilities include RAII guards for semaphores, async semaphores, async condition variables and async thread-safe wrappers and handles.

Coma depends on Boost.ASIO (executors, timers and utilities) and Boost.Beast (initiating function and composed operation utilities only) and familiarity with executors and completion handlers is a prerequisite. An exception is made for the semaphore guards, which only depend on the standard library and can be used without bringing in Boost.

To consume it you can add `include` to your include directories, use `add_subdirectory` from your CMake project, or (TODO) use the Conan package manager.

The library provides:

* `coma::async_semaphore` lightweight async semaphore, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks.
* `coma::async_cond_var` lightweight async condition variable, _not_ thread-safe, no additional synchronization, atomics or reference counting. With FIFO ordering of waiting tasks and without spurious wakening.
* `coma::acquire_guard` equivalent to `std::lock_guard` for semaphores using acquire/release instead of lock/unlock.
* `coma::unique_acquire_guard` equivalent to `std::unique_lock` for semaphores using acquire/release instead of lock/unlock.

And currently experimental or on the TODO list:
* `coma::async_semaphore_timed` with timed functions.
* `coma::async_cond_var_timed` with timed functions, cancellation.
* `coma::async_synchronized` thread-safe async wrapper of values through a strand (similar to proposed `std::synchronized_value`).
* `coma::co_lift` a higher-order function to lift regular functions `R()` into `awaitable<R>()`.

## Examples

Using async semaphore as a lightweight async latch between two threads. This example spawns a new thread to execute some heavy task without blocking the current executor/execution context.
```c++
 // may throw
int blocking_get_answer()
{
    //simulate CPU bound or blocking operation
    std::this_thread::sleep_for(1s);
    return 42;
}

// executor f on a new thread without blocking current executor
template<class F, class R = decltype(f())>
auto co_spawn_thread(F f) -> awaitable<R>
{
    coma::async_semaphore_s sem{co_await this_coro::executor, 0};
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
    co_await sem.async_acquire(use_awaitable);
    if (e)
        std::rethrow_exception(e);
    co_return std::move(ret);
}

awaitable<int> get_answer()
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
    using executor_type = typename coma::async_cond_var_timed::executor_type;
    explicit async_queue(const executor_type& ex) : cv{ex}
    {}
    awaitable<T> async_pop()
    {
        co_await cv.async_wait([&] { return !q.empty(); }));
        auto item = std::move(q.front());
        q.pop_front();
        co_return std::move(item); // TODO move needed?
    }
    awaitable<std::optional<T>> async_try_pop_for(auto timeout)
    {
        if (!co_await cv.async_wait_for(timeout, [&] { return !q.empty(); })))
            co_return std::nullopt;
        auto item = std::move(q.front());
        q.pop_front();
        co_return std::move(item);
    }
    void push(T item)
    {
        q.push_back(std::move(item));
    }
private:
    // NOTE could use the more efficient async_cond_var
    // if timed waits are not required
    coma::async_cond_var_timed cv;
    std::queue<T> q;
};
```

We now make a thread-safe async queue based on the above queue and `coma::stranded`:
```c++
template<class T>
class async_queue_s
{
public:
    using executor_type = typename async_queue::executor_type;
    explicit async_queue_s(const executor_type& ex) : st{ex, std::in_place, ex}
    {}
    awaitable<T> async_pop()
    {
        return st.invoke([](auto& q) { return q.async_pop(); });
        // TODO is this safe?
        // NOTE this is the non-coroutine "awaitable backwarding" variant of
        // co_return co_await st.invoke([](auto& q) -> awaitable<T> {
        //   co_return co_await q.async_pop(); });
    }
    awaitable<std::optional<T>> async_try_pop_for(auto timeout)
    {
        return st.invoke([timeout](auto& q)
        {
            return q.async_try_pop_for(timeout);
        });
    }
    awaitable<void> async_push(T item)
    {
        return st.invoke([item = std::move(item)](auto& q) mutable
        {
            q.push(std::move(item));
        });
    }
private:
    coma::stranded<async_queue> st;
};
```

Async condition variable with timeout and cancellation support
```c++
coma::async_timed_cond_var cv;
bool set;
int answer;

awaitable<int> f(std::stop_token st)
{
    if (co_await cv.async_wait_for(1s, [&] { return set; }, st))
        co_return answer;
    else if (st.stop_requested())
        throw std::runtime_error("cancelled");
    else
        throw std::runtime_error("timeout");
}
```

## Overview and comparison

There are a few variant of semaphores and condition variables where there is a tradeoff between the guarantees the types make and completeness of the API versus performance.

| Type                      | Async | TS\* | Timeout |
|---------------------------|-------|-------|------|
| `std::counting_semaphore` | No | **Yes** | **Yes** |
| `coma::async_semaphore` | **Yes** | No | No |
| `coma::async_semaphore_timed` | **Yes** | No | **Yes** |
| `coma::async_semaphore_s`? | **Yes** | **Yes** | No |
| `coma::async_semaphore_timed_s` | **Yes** | **Yes** | **Yes** |

| Type                      | Async | TS\* | Timeout | Cancellation | SW\* |
|---------------------------|-------|-------|------|------|------|
| `std::conndition_variable` | No | **Yes** | **Yes** | No | **Yes** |
| `std::conndition_variable_any` | No | **Yes** | **Yes** | **Yes** | **Yes** |
| `coma::async_cond_var` | **Yes** | No | No | No | No |
| `coma::async_cond_var_timed` | **Yes** | No | **Yes** | **Yes** | **Yes** |

\* TS = thread-safe, SW = spurious wakeup

## Synopsis

In header `<coma/async_semaphore.hpp>`
```c++
template<class Executor = net::any_io_executor>
class async_semaphore;
```

In header `<coma/async_cond_var.hpp>`
```c++
template<class Executor = net::any_io_executor>
class async_cond_var;
```

In header `<coma/acquire_guard.hpp>`
```c++
template<class Semaphore>
class acquire_guard;
```

In header `<coma/unique_acquire_guard.hpp>`
```c++
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
