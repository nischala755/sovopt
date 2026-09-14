#include <catch2/catch_test_macros.hpp>
#include <span>
#include <vector>

TEST_CASE("C++20 test executable runs with active assertions", "[infrastructure]") {
    const std::vector<int> values{2, 3};
    const std::span<const int> view{values};
    REQUIRE(view.size() == 2);
    REQUIRE(view.front() + view.back() == 5);
}
