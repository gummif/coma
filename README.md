# coma

This library contains async/coroutine/concurrency primatives for C++20. It extends and is based on the standard library and Boost.ASIO. Utilities include RAII guards for semaphores, async semaphores and async condition variables.

## Examples

Blocking semaphores with RAII guard
```c++
std::counting_semaphore<10> sem{10};
int get_anwser();
int f()
{
    coma::acq_rel_guard g{sem};
    return get_anwser(); // may throw
}
```

Async semaphores with RAII guard
```c++
coma::async_semaphore sem{10};
boost::asio::awaitable<int> get_anwser();
boost::asio::awaitable<int> f()
{
    auto g = co_await coma::async_acq_rel_guard(sem);
    return co_await get_anwser(); // may throw
}
```

## Synopsis

