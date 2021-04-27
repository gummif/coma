#pragma once

#include <coma/detail/core_async.hpp>
#if defined(COMA_COROUTINES)

#include <boost/asio/awaitable.hpp>
#include <utility>

namespace coma {

template<class Function>
auto co_lift(Function f) -> boost::asio::awaitable<decltype(std::move(f)())>
{
	co_return std::move(f)();
}

} // namespace coma

#endif
