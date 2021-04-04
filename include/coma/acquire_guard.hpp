#pragma once

#include <coma/guard_tags.hpp>

#include <cassert>
#include <utility>

namespace coma
{

// Call release on semaphore at scope exit
template<class Semaphore>
class acquire_guard
{
public:
    using semaphore_type = Semaphore;

    [[nodiscard]] explicit acquire_guard(Semaphore& sem)
        : m_sem{&sem}
    {
        m_sem->acquire();
    }
    [[nodiscard]] acquire_guard(Semaphore& sem, adapt_acquire_t) noexcept
        : m_sem{&sem}
    {
    }

    ~acquire_guard() noexcept
    {
        m_sem->release();
    }

    acquire_guard(const acquire_guard&) = delete;
    acquire_guard(acquire_guard&&) = delete;

    acquire_guard& operator=(const acquire_guard&) = delete;
    acquire_guard& operator=(acquire_guard&&) = delete;

private:
    Semaphore* m_sem;
};

} // namespace coma
