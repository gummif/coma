#include <coma/acq_rel_guard.hpp>
#include <catch2/catch.hpp>

TEST_CASE("acq_rel_guard", "[acq_rel_guard]") {

    struct dummy
    {
        int n = 1;
        void acquire() { --n; }
        void release() { ++n; }
    } d;

    CHECK(d.n == 1);
    {
        coma::acq_rel_guard g{d};
        CHECK(d.n == 0);
    }
    CHECK(d.n == 1);

    {
        coma::acq_rel_guard g{d, coma::adapt_acquire};
        CHECK(d.n == 1);
    }
    CHECK(d.n == 2);
}
