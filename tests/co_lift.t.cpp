#include <coma/co_lift.hpp>
#include <iostream>
#include <test_util.hpp>
#include <vector>
#if defined(COMA_COROUTINES) && defined(COMA_ENABLE_COROUTINE_TESTS)

TEST_CASE("co_lift", "[co_lift]")
{
	boost::asio::io_context ctx;
	std::vector<int> v;

	boost::asio::co_spawn(ctx, coma::co_lift([&] { v.push_back(42); }), boost::asio::detached);

	boost::asio::co_spawn(
		ctx,
		[&]() -> boost::asio::awaitable<void> {
			TEST_LOG("before await");
			auto val = co_await coma::co_lift([&, l = coma::logged()]() {
				TEST_LOG("inside lifted fn");
				v.push_back(43);
				return 0;
			});
			TEST_LOG("after await");
			CHECK(val == 0);
			TEST_LOG("before await");
			val = co_await coma::co_lift([&, l = coma::logged()]() { return 2; });
			TEST_LOG("after await");
			CHECK(val == 2);
		},
		boost::asio::detached);

	ctx.run();

	CHECK(v[0] == 42);
	CHECK(v[1] == 43);
}

#endif // defined(COMA_ENABLE_COROUTINE_TESTS)
