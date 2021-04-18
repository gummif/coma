#include <coma/unique_acquire_guard.hpp>
#include <test_util.hpp>

struct dummy
{
    int n;

    void acquire() { --n; }
    bool try_acquire() {
        if (n) {
            --n;
            return true;
        }
        return false;
    }
    template<class T>
    bool try_acquire_for(T t) {
        return try_acquire_until(defclock::now() + t);
    }
    template<class T>
    bool try_acquire_until(T t) {
        while (n == 0 && defclock::now() < t)
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        if (n) {
            --n;
            return true;
        }
        return false;
    }
    void release() { ++n; }
};

TEST_CASE("unique_acquire_guard", "[unique_acquire_guard]")
{
    using guard_t = coma::unique_acquire_guard<dummy>;
    dummy d{1};

    CHECK(d.n == 1);
    {
        guard_t g;
    }
    {
        guard_t g{d};
        CHECK(d.n == 0);
        CHECK(g);
        CHECK(g.owns_acquire());
        CHECK(g.semaphore() == &d);
    }
    CHECK(d.n == 1);
    {
        guard_t g{d};
        CHECK(d.n == 0);
    }
    CHECK(d.n == 1);
    {
        guard_t g{d};
        CHECK(d.n == 0);
        g.release();
        CHECK(!g.owns_acquire());
        CHECK(d.n == 1);
    }
    CHECK(d.n == 1);
    {
        guard_t g{d};
        CHECK(d.n == 0);
        CHECK(g.release_ownership() == &d);
        CHECK(!g.owns_acquire());
        CHECK(g.semaphore() == nullptr);
        CHECK(d.n == 0);
    }
    CHECK(d.n == 0);
    d.n = 1;

    {
        guard_t g{d, coma::adapt_acquire};
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(d.n == 2);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d};
        CHECK(d.n == 1);
        coma::unique_acquire_guard<dummy> g2;
        using std::swap;
        swap(g, g2);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d};
        CHECK(d.n == 1);
        auto g2 = std::move(g);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d};
        CHECK(d.n == 1);
        coma::unique_acquire_guard<dummy> g2;
        g = std::move(g2);
        CHECK(d.n == 2);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK_THROWS_AS(g.release(), std::system_error);
        g.acquire();
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire() == true);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire_for(std::chrono::seconds{1}) == true);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire_until(defclock::now() + std::chrono::seconds{1}) == true);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    d.n = 0;
    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire() == false);
        CHECK(d.n == 0);
    }
    CHECK(d.n == 0);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire_for(std::chrono::milliseconds{1}) == false);
        CHECK(d.n == 0);
    }
    CHECK(d.n == 0);

    {
        guard_t g{d, coma::defer_acquire};
        CHECK(g.try_acquire_until(defclock::now() + std::chrono::milliseconds{1}) == false);
        CHECK(d.n == 0);
    }
    CHECK(d.n == 0);
}
