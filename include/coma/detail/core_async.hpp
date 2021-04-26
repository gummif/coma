#pragma once

#include <boost/asio/async_result.hpp>
#include <boost/version.hpp>
#include <coma/detail/core.hpp>

#if defined(__cpp_impl_coroutine) && __cplusplus > 201703L
#define COMA_COROUTINES
#endif

#define COMA_ASYNC_RETURN_EC                                                                  \
	typename net::async_result<typename std::decay<CompletionToken>::type,                    \
							   void(boost::system::error_code)>::return_type

#if BOOST_VERSION >= 107400
#define COMA_HAS_DEFAULT_IO_EXECUTOR
#define COMA_SET_DEFAULT_IO_EXECUTOR = net::any_io_executor
#else
#define COMA_SET_DEFAULT_IO_EXECUTOR // no default pre Boost 1.74
#endif

// fails on 1.76 with msvc
#if BOOST_VERSION >= 107400 && !_WIN32
#define COMA_HAS_AS_DEFAULT_ON
#endif

namespace boost {
namespace asio {
}
namespace beast {
}
} // namespace boost

namespace coma {
namespace net {
using namespace boost::asio;
} // namespace net
namespace netext {
using namespace boost::beast;
} // namespace netext
} // namespace coma
