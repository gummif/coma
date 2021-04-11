#pragma once

#include <coma/guard_tags.hpp>

#include <cassert>
#include <chrono>
#include <system_error>
#include <utility>

namespace coma
{

// Call release on semaphore at scope exit if owns acquire
template<class Semaphore>
class unique_acquire_guard
{
public:
    using semaphore_type = Semaphore;

    unique_acquire_guard() noexcept = default;

    [[nodiscard]] explicit unique_acquire_guard(Semaphore& sem)
        : m_sem{&sem}
    {
        m_sem->acquire();
        m_active = true;
    }

    [[nodiscard]] unique_acquire_guard(Semaphore& sem, defer_acquire_t) noexcept
        : m_sem{&sem}
        , m_active{false}
    {
    }

    [[nodiscard]] unique_acquire_guard(Semaphore& sem, try_to_acquire_t) noexcept
        : m_sem{&sem}
    {
        m_active = m_sem->try_acquire();
    }

    [[nodiscard]] unique_acquire_guard(Semaphore& sem, adapt_acquire_t)
        : m_sem{&sem}
        , m_active{true}
    {
    }

    template<class Rep, class Period>
    [[nodiscard]] unique_acquire_guard(Semaphore& sem,
        const std::chrono::duration<Rep,Period>& timeout_duration)
        : m_sem{&sem}
    {
        m_active = m_sem->try_acquire_for(timeout_duration);
    }

    template<class Clock, class Duration>
    [[nodiscard]] unique_acquire_guard(Semaphore& sem,
        const std::chrono::time_point<Clock,Duration>& timeout_time)
        : m_sem{&sem}
    {
        m_active = m_sem->try_acquire_until(timeout_time);
    }

    unique_acquire_guard(const unique_acquire_guard&) = delete;

    [[nodiscard]] unique_acquire_guard(unique_acquire_guard&& o) noexcept
        : m_sem{std::exchange(o.m_sem, nullptr)}
        , m_active{std::exchange(o.m_active, false)}
    {}

    ~unique_acquire_guard()
    {
        if (m_sem && m_active)
            m_sem->release();
    }

    unique_acquire_guard& operator=(const unique_acquire_guard&) = delete;

    unique_acquire_guard& operator=(unique_acquire_guard&& o) noexcept
    {
        if (m_sem && m_active)
            m_sem->release();
        m_sem = std::exchange(o.m_sem, nullptr);
        m_active = std::exchange(o.m_active, false);
        return *this;
    };

    void acquire()
    {
        if (!m_sem)
            throw_system_error(std::errc::operation_not_permitted, "unique_acquire_guard::acquire(): no associated semaphore");
        if (m_active)
            throw_system_error(std::errc::resource_deadlock_would_occur, "unique_acquire_guard::acquire(): already owns acquire");
        m_sem->acquire();
        m_active = true;
    }

    [[nodiscard]]
    bool try_acquire()
    {
        if (!m_sem)
            throw_system_error(std::errc::operation_not_permitted, "unique_acquire_guard::try_acquire(): no associated semaphore");
        if (m_active)
            throw_system_error(std::errc::resource_deadlock_would_occur, "unique_acquire_guard::try_acquire(): already owns acquire");
        m_active = m_sem->try_acquire();
        return m_active;
    }

    template<class Rep, class Period>
    [[nodiscard]]
    bool try_acquire_for(const std::chrono::duration<Rep,Period>& timeout_duration)
    {
        if (!m_sem)
            throw_system_error(std::errc::operation_not_permitted, "unique_acquire_guard::try_acquire_for(): no associated semaphore");
        if (m_active)
            throw_system_error(std::errc::resource_deadlock_would_occur, "unique_acquire_guard::try_acquire_for(): already owns acquire");
        m_active = m_sem->try_acquire_for(timeout_duration);
        return m_active;
    }

    template<class Clock, class Duration>
    [[nodiscard]]
    bool try_acquire_until(const std::chrono::time_point<Clock,Duration>& timeout_time)
    {
        if (!m_sem)
            throw_system_error(std::errc::operation_not_permitted, "unique_acquire_guard::try_acquire_until(): no associated semaphore");
        if (m_active)
            throw_system_error(std::errc::resource_deadlock_would_occur, "unique_acquire_guard::try_acquire_until(): already owns acquire");
        m_active = m_sem->try_acquire_until(timeout_time);
        return m_active;
    }

    void release()
    {
        if (!owns_acquire())
            throw_system_error(std::errc::operation_not_permitted, "unique_acquire_guard::release(): does not own acquire");
        m_sem->release();
        m_active = false;
    }

    void swap(unique_acquire_guard& other) noexcept
    {
        std::swap(m_sem, other.m_sem);
        std::swap(m_active, other.m_active);
    }

    // equivalent to std::unique_lock::release()
    [[nodiscard]] semaphore_type* release_ownership() noexcept
    {
        m_active = false;
        return std::exchange(m_sem, nullptr);
    }

    [[nodiscard]] semaphore_type* semaphore() const noexcept { return m_sem; }

    [[nodiscard]] bool owns_acquire() const noexcept { return m_sem && m_active; }

    explicit operator bool() const noexcept { return owns_acquire(); }

private:
    Semaphore* m_sem{nullptr};
    bool m_active{false};

    [[noreturn]] void throw_system_error(std::errc ev, const char* what)
    {
        throw std::system_error(std::error_code(static_cast<int>(ev), std::system_category()), what);
    }
};

template<class Sem>
void swap(unique_acquire_guard<Sem>& a, unique_acquire_guard<Sem>& b) noexcept
{
    a.swap(b);
}

} // namespace coma
