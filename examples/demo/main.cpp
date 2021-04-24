#include <coma/async_cond_var.hpp>
#include <boost/asio/io_context.hpp>

int main()
{
    boost::asio::io_context ctx;
    coma::async_cond_var<> cv{ctx.get_executor()};
    return 0;
}