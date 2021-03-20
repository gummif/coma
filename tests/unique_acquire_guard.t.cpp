#include <coma/unique_acquire_guard.hpp>
#include <catch2/catch.hpp>

TEST_CASE("unique_acquire_guard", "[unique_acquire_guard]") {

    struct dummy
    {
        int n = 1;
        void acquire() { --n; }
        void release() { ++n; }
    } d;

    CHECK(d.n == 1);
    {
        coma::unique_acquire_guard<dummy> g;
    }
    {
        coma::unique_acquire_guard g{d};
        CHECK(d.n == 0);
    }
    CHECK(d.n == 1);

    {
        coma::unique_acquire_guard g{d, coma::adapt_acquire};
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        coma::unique_acquire_guard g{d, coma::defer_acquire};
        CHECK(d.n == 2);
    }
    CHECK(d.n == 2);

    {
        coma::unique_acquire_guard g{d};
        CHECK(d.n == 1);
        coma::unique_acquire_guard<dummy> g2;
        using std::swap;
        swap(g, g2);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        coma::unique_acquire_guard g{d};
        CHECK(d.n == 1);
        auto g2 = std::move(g);
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);

    {
        coma::unique_acquire_guard g{d};
        CHECK(d.n == 1);
        coma::unique_acquire_guard<dummy> g2;
        g = std::move(g2);
        CHECK(d.n == 2);
    }
    CHECK(d.n == 2);
}
