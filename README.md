# `coma`

This library contains async/coroutine/concurrency primatives for C++20. It extends and is based on the standard library and Boost.ASIO. Utilities include RAII guards for semaphores, async semaphores and async condition variables.

## Examples

Blocking semaphores with RAII guard
```c++
std::counting_semaphore<10> sem{10};
int get_answer(); // may throw

int f()
{
    coma::acquire_guard g{sem};
    return get_answer();
}
```

Async semaphores with RAII guard
```c++
coma::async_semaphore sem{10};
boost::asio::awaitable<int> get_answer(); // may throw

boost::asio::awaitable<int> f()
{
    auto g = co_await coma::async_acquire_guard(sem);
    co_return co_await get_answer();
}
```

## Synopsis

```c++
// The semaphore equivalent of std::lock_guard
// using acquire/release instead of lock/unlock.
template<class Semaphore>
class acquire_guard;
```
```c++
// The semaphore equivalent of std::unique_lock
template<class Semaphore>
class unique_acquire_guard;
```