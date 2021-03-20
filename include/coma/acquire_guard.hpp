#include <coma/guard_tags.hpp>

#include <cassert>
#include <utility>

namespace coma
{

// Call release on semaphore at scope exit
template<class Sem>
class acquire_guard
{
public:
    using semaphore_type = Sem;

    [[nodiscard]] explicit acquire_guard(Sem& sem)
        : m_sem{&sem}
    {
        m_sem->acquire();
    }
    [[nodiscard]] acquire_guard(Sem& sem, adapt_acquire_t) noexcept
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
    Sem* m_sem;
};

// Call async_release (detached) on semaphore at scope exit
// if you need guarantee that release has been called exactly at scoped exit then you need to do that manually
template<class Sem>
class async_scoped_acquire_guard
{
public:
    [[nodiscard]] async_scoped_acquire_guard(Sem& sem, adapt_acquire_t)
        : m_sem{sem}
    {
    }

    ~async_scoped_acquire_guard()
    {
        // BLE
        //asio::co_spawn(m_sem.get_executor(), m_sem.async_release(), asio::detached);
    }

    async_scoped_acquire_guard(const async_scoped_acquire_guard&) = delete;
    async_scoped_acquire_guard& operator=(const async_scoped_acquire_guard&) = delete;

private:
    Sem& m_sem;
};

} // namespace coma
