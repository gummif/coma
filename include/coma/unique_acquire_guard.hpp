#include <coma/guard_tags.hpp>

#include <cassert>
#include <chrono>
#include <system_error>
#include <utility>

namespace coma
{

// Call release on semaphore at scope exit if owns acquire
template<class Sem>
class unique_acquire_guard
{
public:
    using semaphore_type = Sem;

    unique_acquire_guard() {}

    [[nodiscard]] explicit unique_acquire_guard(Sem& sem)
        : m_sem{&sem}
    {
        m_sem->acquire();
        m_active = true;
    }

    [[nodiscard]] unique_acquire_guard(Sem& sem, defer_acquire_t) noexcept
        : m_sem{&sem}
        , m_active{false}
    {
    }

    [[nodiscard]] unique_acquire_guard(Sem& sem, try_to_acquire_t) noexcept
        : m_sem{&sem}
    {
        m_active = m_sem->try_acquire();
    }

    [[nodiscard]] unique_acquire_guard(Sem& sem, adapt_acquire_t)
        : m_sem{&sem}
        , m_active{true}
    {
    }

    template<class Rep, class Period>
    [[nodiscard]] unique_acquire_guard(Sem& sem,
        const std::chrono::duration<Rep,Period>& timeout_duration)
        : m_sem{&sem}
    {
        m_active = m_sem->try_acquire_for(timeout_duration);
    }

    template<class Clock, class Duration>
    [[nodiscard]] unique_acquire_guard(Sem& sem,
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

    ~unique_acquire_guard() noexcept
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

    void release()
    {
        if (m_sem && m_active)
        {
            m_sem->release();
            m_active = false;
        }
        else
        {
            throw std::system_error(std::errc::operation_not_permitted);
        }
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
    Sem* m_sem{nullptr};
    bool m_active{false};
};

template<class Sem>
void swap(unique_acquire_guard<Sem>& a, unique_acquire_guard<Sem>& b) noexcept
{
    a.swap(b);
}

} // namespace coma
