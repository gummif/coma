#pragma once

#include <utility>
#include <asio/awaitable.hpp>

namespace coma
{

template<class Function>
auto co_lift(Function f) -> asio::awaitable<decltype(std::move(f)())>
{
    co_return std::move(f)();
}

} // namespace coma
