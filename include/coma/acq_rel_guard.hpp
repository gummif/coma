#include <coma/guard_tags.hpp>

#include <cassert>
#include <utility>

namespace coma
{
    // Call release on semaphore at scope exit
    template<class Sem>
    class acq_rel_guard
    {
    public:
        using semaphore_type = Sem;

        [[nodiscard]] explicit acq_rel_guard(Sem& sem)
            : m_sem{&sem}
        {
            m_sem->acquire();
        }
        [[nodiscard]] acq_rel_guard(Sem& sem, adapt_acquire_t) noexcept
            : m_sem{&sem}
        {
        }

        ~acq_rel_guard() noexcept
        {
            m_sem->release();
        }

        acq_rel_guard(const acq_rel_guard&) = delete;
        acq_rel_guard(acq_rel_guard&&) = delete;

        acq_rel_guard& operator=(const acq_rel_guard&) = delete;
        acq_rel_guard& operator=(acq_rel_guard&&) = delete;

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

    // Call release on semaphore at scope exit
    template<class Sem>
    class unique_acquire_guard
    {
    public:
        [[nodiscard]] unique_acquire_guard(Sem& sem, adapt_acquire_t)
            : m_sem{&sem}
        {
        }

        unique_acquire_guard(const unique_acquire_guard&) = delete;

        [[nodiscard]] unique_acquire_guard(unique_acquire_guard&& o)
            : m_sem{std::exchange(o.m_sem, nullptr)}
        {}

        ~unique_acquire_guard()
        {
            if (m_sem)
                m_sem->release();
        }

        unique_acquire_guard& operator=(const unique_acquire_guard&) = delete;

        unique_acquire_guard& operator=(unique_acquire_guard&& o)
        {
            if (m_sem)
                m_sem->release();
            m_sem = std::exchange(o.m_sem, nullptr);
        };
    private:
        Sem* m_sem;
    };

    // Call release on semaphore at scope exit
    template<class Sem>
    class unique_acquire_guard_n
    {
    public:
        [[nodiscard]] unique_acquire_guard_n(Sem& sem, adapt_acquire_t, std::ptrdiff_t n = 1)
            : m_sem{&sem}
            , m_n{n}
        {
            assert(m_n >= 0);
        }

        unique_acquire_guard_n(const unique_acquire_guard_n&) = delete;

        [[nodiscard]] unique_acquire_guard_n(unique_acquire_guard_n&& o)
            : m_sem{std::exchange(o.m_sem, nullptr)}
            , m_n{std::exchange(o.m_n, 0)}
        {}

        ~unique_acquire_guard_n()
        {
            if (m_sem)
                m_sem->release(m_n);
        }

        unique_acquire_guard_n& operator=(const unique_acquire_guard_n&) = delete;

        unique_acquire_guard_n& operator=(unique_acquire_guard_n&& o)
        {
            if (m_sem)
                m_sem->release(m_n);
            m_sem = std::exchange(o.m_sem, nullptr);
            m_n = std::exchange(o.m_n, 0);
        };
    private:
        Sem* m_sem;
        std::ptrdiff_t m_n;
    };

} // namespace coma
