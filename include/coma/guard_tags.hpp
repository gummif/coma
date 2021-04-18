#pragma once

#include <coma/detail/core.hpp>

namespace coma
{
    struct adapt_acquire_t {};
    struct try_to_acquire_t {};
    struct defer_acquire_t {};
    COMA_INLINE_VAR constexpr adapt_acquire_t adapt_acquire{};
    COMA_INLINE_VAR constexpr try_to_acquire_t try_to_acquire{};
    COMA_INLINE_VAR constexpr defer_acquire_t defer_acquire{};
} // namespace coma
