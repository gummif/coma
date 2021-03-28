# `coma`

This library contains async/coroutine/concurrency primatives for C++20 built on Boost.ASIO. Utilities include RAII guards for semaphores, async semaphores, async condition variables and async thread-safe wrappers and handles.

The library provides:
* `coma::acquire_guard` and `coma::unique_acquire_guard` for semaphores, equivalent to `std::lock_guard` and `std::unique_lock` for mutexes.
* `coma::async_semaphore` and `coma::async_timed_semaphore` (and `*_mt` variants) semaphores with async APIs.
* `coma::async_cond_var` and `coma::async_timed_cond_var` condition variables with async APIs.
* `coma::stranded` thread-safe async wrapper of values through a strand (similar to proposed `std::synchronized_value`).

## Examples

Blocking semaphore with RAII guard
```c++
std::counting_semaphore<10> sem{10};
int get_answer(); // may throw

int f()
{
    coma::acquire_guard g{sem};
    return get_answer();
}
```

Async semaphore with RAII guard
```c++
coma::async_semaphore sem{10};
awaitable<int> get_answer(); // may throw

awaitable<int> f()
{
    auto g = co_await coma::async_acquire_guard(sem);
    co_return co_await get_answer();
}
```

Async semaphore with RAII guard and timeout
```c++
coma::async_timed_semaphore sem{10};
awaitable<int> get_answer(); // may throw

awaitable<int> f()
{
    if (!co_await sem.async_acquire_for(1s))
        throw std::runtime_error("timeout");
    coma::acquire_guard g{sem, adapt_acquire};
    co_return co_await get_answer();
}
```

An async queue is almost trivial to write with an async condition variable
```c++
template<class T>
class async_queue
{
public:
    using executor_type = typename coma::async_timed_cond_var::executor_type;
    explicit async_queue(const executor_type& ex) : cv{ex}
    {}
    awaitable<T> pop()
    {
        co_await cv.async_wait([&] { return !q.empty(); }));
        auto item = std::move(q.front());
        q.pop_front();
        co_return item;
    }
    awaitable<std::optional<T>> try_pop_for(auto timeout)
    {
        if (!co_await cv.async_wait_for(timeout, [&] { return !q.empty(); })))
            return std::nullopt;
        auto item = std::move(q.front());
        q.pop_front();
        co_return item;
    }
    void push(T item)
    {
        q.push_back(std::move(item));
    }
private:
    // NOTE could use the more efficient async_cond_var
    // if timed waits are not required
    coma::async_timed_cond_var cv;
    std::queue<T> q;
};
```

We now make a thread-safe async queue based on the above queue and `coma::stranded`:
```c++
template<class T>
class async_queue_mt
{
public:
    using executor_type = typename async_queue::executor_type;
    explicit async_queue_mt(const executor_type& ex) : st{ex, std::in_place, ex}
    {}
    awaitable<T> pop()
    {
        return st.invoke([](auto& cv) { return cv.pop(); });
    }
    awaitable<std::optional<T>> try_pop_for(auto timeout)
    {
        return st.invoke([timeout](auto& cv)
        {
            return cv.try_pop_for(timeout);
        });
    }
    awaitable<void> push(T item)
    {
        return st.invoke([item = std::move(item)](auto& cv) mutable
        {
            cv.push(std::move(item));
        });
    }
private:
    stranded<async_queue> st;
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
| `coma::async_timed_semaphore` | **Yes** | No | **Yes** |
| `coma::async_semaphore_mt` | **Yes** | **Yes** | No |
| `coma::async_timed_semaphore_mt` | **Yes** | **Yes** | **Yes** |

| Type                      | Async | TS\* | Timeout | Cancellation | SW\* |
|---------------------------|-------|-------|------|------|------|
| `std::conndition_variable` | No | **Yes** | **Yes** | No | **Yes** |
| `std::conndition_variable_any` | No | **Yes** | **Yes** | **Yes** | **Yes** |
| `coma::async_cond_var` | **Yes** | No | No | No | No |
| `coma::async_timed_cond_var` | **Yes** | No | **Yes** | **Yes** | **Yes** |

\* TS = thread-safe, SW = spurious wakeup

## Nomenclature

We can try to classify functions that do multi-thread or multi-task synchronization into:

* *Async/asynchronous*: Deferred function continuation. Can be
    * deferred blocking (`awaitable<void> async_lock()`)
    * deferred non-blocking  (`awaitable<bool> async_try_lock()`)
* *Synchronous*: Inline function completion. Can be
    * blocking (`void lock()`) or 
    * non-blocking (`bool try_lock()`).

Async vs sync in general says nothing of the thread-safety of a function. Functions in this library should be considered non-thread-safe unless specified otherwise. Classes with a `_mt` suffix provide a thread-safe API.

## Synopsis

### `coma::acquire_guard`
```c++
template<class Semaphore>
class acquire_guard;
```
The semaphore equivalent of `std::lock_guard` using acquire/release instead of lock/unlock.

### `coma::unique_acquire_guard`
```c++
template<class Semaphore>
class unique_acquire_guard;
```
The semaphore equivalent of `std::unique_lock` using acquire/release instead of lock/unlock.

### `coma::async_acquire_guard`
```c++
template<class Semaphore>
boost::asio::awaitable<acquire_guard>
  async_acquire_guard(Semaphore& sem);
```
Acquire semaphore using `sem.async_acquire()` and return an `acquire_guard` (async RAII). Equivalent to `co_await sem.async_acquire()` followed by `acquire_guard(sem, adapt_acquire)`.

TODO timeout support in async_acquire_guard, maybe there should these should rather be mem fns of unique_lock?
