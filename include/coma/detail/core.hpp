#pragma once

#include <utility>
#include <type_traits>

namespace boost {
namespace asio {
}
namespace beast {
}
}

namespace coma {

namespace net {
using namespace boost::asio;
} // namespace net
namespace netext {
using namespace boost::beast;
} // namespace netext

namespace detail {

template<class T>
using void_t = void;

template<class T, class = void>
struct is_predicate : std::false_type
{
};

template<class T>
struct is_predicate<
  T,
  void_t<decltype(std::declval<T>()() == true)>>
    : std::true_type
{
};

} // namespace detail
} // namespace coma
