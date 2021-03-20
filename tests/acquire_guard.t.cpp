#include <coma/acquire_guard.hpp>
#include <catch2/catch.hpp>

TEST_CASE("acquire_guard", "[acquire_guard]") {

    struct dummy
    {
        int n = 1;
        void acquire() { --n; }
        void release() { ++n; }
    } d;

    CHECK(d.n == 1);
    {
        coma::acquire_guard g{d};
        CHECK(d.n == 0);
    }
    CHECK(d.n == 1);

    {
        coma::acquire_guard g{d, coma::adapt_acquire};
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);
}
