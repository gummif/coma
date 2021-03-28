#include <coma/async_seq_cond_var.hpp>
#include <catch2/catch.hpp>

TEST_CASE("async_seq_cond_var", "[async_seq_cond_var]")
{

    asio::io_context ctx;
    coma::async_seq_cond_var cv{ctx.get_executor()};

}
