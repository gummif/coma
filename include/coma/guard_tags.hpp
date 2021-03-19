
namespace coma
{
    struct adapt_acquire_t {};
    struct try_to_acquire_t {};
    struct defer_acquire_t {};
    inline constexpr adapt_acquire_t adapt_acquire{};
    inline constexpr try_to_acquire_t try_to_acquire{};
    inline constexpr defer_acquire_t defer_acquire{};
} // namespace coma
