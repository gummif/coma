#pragma once

#include <utility>
#include <boost/asio/awaitable.hpp>

namespace coma
{

template<class Function>
auto co_lift(Function f) -> boost::asio::awaitable<decltype(std::move(f)())>
{
    co_return std::move(f)();
}

} // namespace coma