#include <catch2/catch_test_macros.hpp>
#include <sovereign/sparse_matrix.hpp>
#include <sovereign/errors.hpp>
#include <limits>
#include <vector>
using namespace sovereign;

TEST_CASE("CSC canonicalizes unordered duplicates and removes exact zeros", "[matrix]") {
    const auto a = CscMatrix::from_triplets(3, 2, {{2,1,5}, {0,0,2}, {1,0,3}, {1,0,-3}, {0,0,1}, {2,0,0}});
    REQUIRE(a.rows() == 3);
    REQUIRE(a.columns() == 2);
    REQUIRE(a.nonzeros() == 2);
    REQUIRE(std::vector<Index>(a.column_offsets().begin(), a.column_offsets().end()) == std::vector<Index>{0,1,2});
    REQUIRE(a.column(1).rows.front() == 2);
    REQUIRE(a.column(0).values.front() == 3);
    REQUIRE_THROWS_AS(a.column(2), std::out_of_range);
}
TEST_CASE("CSC computes rectangular products and transpose products", "[matrix]") {
    const auto a = CscMatrix::from_triplets(3, 2, {{0,0,2}, {2,0,-1}, {1,1,4}, {2,1,3}});
    REQUIRE(a.multiply(std::vector<double>{3,2}) == std::vector<double>{6,8,3});
    REQUIRE(a.transpose_multiply(std::vector<double>{1,2,3}) == std::vector<double>{-1,17});
    REQUIRE_THROWS_AS(a.multiply(std::vector<double>{1}), InvalidModelError);
    REQUIRE_THROWS_AS(a.transpose_multiply(std::vector<double>{1}), InvalidModelError);
}
TEST_CASE("CSC rejects out of range and nonfinite entries including duplicate overflow", "[matrix]") {
    REQUIRE_THROWS_AS(CscMatrix::from_triplets(1,1,{{1,0,1}}), InvalidModelError);
    REQUIRE_THROWS_AS(CscMatrix::from_triplets(1,1,{{0,1,1}}), InvalidModelError);
    REQUIRE_THROWS_AS(CscMatrix::from_triplets(1,1,{{0,0,std::numeric_limits<double>::infinity()}}), InvalidModelError);
    REQUIRE_THROWS_AS(CscMatrix::from_triplets(1,1,{{0,0,std::numeric_limits<double>::quiet_NaN()}}), InvalidModelError);
    REQUIRE_THROWS_AS(CscMatrix::from_triplets(1,1,{{0,0,1e308},{0,0,1e308}}), InvalidModelError);
}
TEST_CASE("CSC supports empty dimensions without dense storage", "[matrix]") {
    const auto a = CscMatrix::from_triplets(1000000000, 1, {});
    REQUIRE(a.nonzeros() == 0);
    REQUIRE(a.column_offsets().size() == 2);
    const auto empty = CscMatrix::from_triplets(0,0,{});
    REQUIRE(empty.multiply({}).empty());
    REQUIRE(empty.transpose_multiply({}).empty());
}
