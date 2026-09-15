#include <catch2/catch_test_macros.hpp>
#include <sovereign/basis.hpp>
#include <cmath>
#include <limits>
#include <vector>
using namespace sovereign;

TEST_CASE("Sparse basis solves row permuted systems and their transpose", "[basis]") {
    const auto matrix = CscMatrix::from_triplets(3, 4,
        {{0,1,2}, {1,0,3}, {1,1,1}, {1,2,4}, {2,0,1}, {2,2,5}, {0,3,7}});
    const SparseBasis basis(matrix, std::vector<Index>{0,1,2});
    const auto x = basis.solve(std::vector<double>{4,17,16});
    const auto y = basis.solve_transpose(std::vector<double>{9,4,23});
    for (Index i = 0; i < 3; ++i) {
        REQUIRE(std::abs(x[i] - static_cast<double>(i + 1)) < 1e-12);
        REQUIRE(std::abs(y[i] - static_cast<double>(i + 1)) < 1e-12);
    }
    REQUIRE(basis.dimension() == 3);
}
TEST_CASE("Sparse basis handles empty and sparse triangular matrices", "[basis]") {
    const SparseBasis empty(CscMatrix{}, {});
    REQUIRE(empty.solve({}).empty());
    REQUIRE(empty.solve_transpose({}).empty());
    REQUIRE(empty.nonzeros() == 0);
    constexpr Index n = 2000;
    std::vector<Triplet> entries;
    std::vector<Index> columns;
    std::vector<double> rhs(n, 3.0);
    for (Index i = 0; i < n; ++i) {
        entries.push_back({i,i,2});
        if (i > 0) entries.push_back({i,i-1,1});
        columns.push_back(i);
    }
    rhs[0] = 2;
    const SparseBasis basis(CscMatrix::from_triplets(n,n,entries), columns);
    REQUIRE(basis.nonzeros() == 2*n-1);
    for (const double value : basis.solve(rhs)) REQUIRE(value == 1);
    rhs[0] = 3;
    rhs[n-1] = 2;
    for (const double value : basis.solve_transpose(rhs)) REQUIRE(value == 1);
}
TEST_CASE("Sparse basis rejects invalid input and numerical breakdown", "[basis]") {
    const auto identity = CscMatrix::from_triplets(2,2,{{0,0,1},{1,1,1}});
    REQUIRE_THROWS_AS(SparseBasis(identity,std::vector<Index>{0}), NumericalError);
    REQUIRE_THROWS_AS(SparseBasis(identity,std::vector<Index>{0,0}), NumericalError);
    REQUIRE_THROWS_AS(SparseBasis(identity,std::vector<Index>{0,2}), NumericalError);
    REQUIRE_THROWS_AS(SparseBasis(identity,std::vector<Index>{0,1},0), NumericalError);
    REQUIRE_THROWS_AS(SparseBasis(identity,std::vector<Index>{0,1},std::numeric_limits<double>::quiet_NaN()), NumericalError);
    const SparseBasis basis(identity,std::vector<Index>{0,1});
    REQUIRE_THROWS_AS(basis.solve(std::vector<double>{1}), NumericalError);
    REQUIRE_THROWS_AS(basis.solve_transpose(std::vector<double>{1}), NumericalError);
    REQUIRE_THROWS_AS(basis.solve(std::vector<double>{1,std::numeric_limits<double>::infinity()}), NumericalError);
    REQUIRE_THROWS_AS(basis.solve_transpose(std::vector<double>{1,std::numeric_limits<double>::quiet_NaN()}), NumericalError);
    REQUIRE_THROWS_AS(SparseBasis(CscMatrix::from_triplets(2,2,{{0,0,1},{0,1,1}}),std::vector<Index>{0,1}), NumericalError);
    const auto tiny = CscMatrix::from_triplets(1,1,{{0,0,1e-15}});
    REQUIRE_THROWS_AS(SparseBasis(tiny,std::vector<Index>{0}), NumericalError);
    REQUIRE(SparseBasis(tiny,std::vector<Index>{0},1e-16).solve(std::vector<double>{1e-15})[0] == 1);
}

TEST_CASE("Sparse basis reports refined forward and transpose residuals", "[basis][numerical]") {
    const auto matrix = CscMatrix::from_triplets(3, 3,
        {{0,0,1e-8},{1,0,1},{1,1,1},{2,1,1},{2,2,1e8}});
    const SparseBasis basis(matrix, std::vector<Index>{0,1,2}, 1e-14);
    const std::vector<double> expected{1,2,3};
    const auto rhs = matrix.multiply(expected);
    const auto forward = basis.solve(rhs);
    const auto forward_info = basis.last_solve_info();
    REQUIRE(forward_info.scaled_residual <= 1e-14);
    REQUIRE(forward_info.refinements <= 3);
    const auto transpose_rhs = matrix.transpose_multiply(expected);
    const auto transpose = basis.solve_transpose(transpose_rhs);
    const auto transpose_info = basis.last_solve_info();
    REQUIRE(transpose_info.scaled_residual <= 1e-14);
    REQUIRE(transpose_info.refinements <= 3);
    for (Index i=0;i<3;++i) {
        REQUIRE(std::abs(forward[i]-expected[i]) <= 1e-8);
        REQUIRE(std::abs(transpose[i]-expected[i]) <= 1e-8);
    }
}

TEST_CASE("Updated sparse basis applies eta updates to forward and transpose solves", "[basis][updates]") {
    const auto a=CscMatrix::from_triplets(3,5,{{0,0,1},{1,1,1},{2,2,1},{0,3,2},{1,3,1},{1,4,3},{2,4,2}});
    UpdatedBasis basis(a,{0,1,2},1e-12,8);
    basis.replace(0,3);
    basis.replace(2,4);
    const std::vector<double> rhs={5,7,11};
    const auto x=basis.solve(rhs);
    const auto xt=basis.solve_transpose(rhs);
    const SparseBasis direct(a,std::vector<Index>{3,1,4});
    const auto expected=direct.solve(rhs);
    const auto expected_t=direct.solve_transpose(rhs);
    REQUIRE(x.size()==expected.size());
    for(Index i=0;i<x.size();++i) REQUIRE(std::abs(x[i]-expected[i]) <= 1e-11);
    for(Index i=0;i<xt.size();++i) REQUIRE(std::abs(xt[i]-expected_t[i]) <= 1e-11);
    REQUIRE(basis.update_count()==2);
    REQUIRE(basis.refactorizations()==1);
}

TEST_CASE("Updated sparse basis refactorizes at its update limit and rejects weak pivots", "[basis][updates]") {
    const auto a=CscMatrix::from_triplets(2,4,{{0,0,1},{1,1,1},{0,2,1},{1,2,1},{0,3,1e-15},{1,3,1}});
    UpdatedBasis basis(a,{0,1},1e-12,1);
    basis.replace(0,2);
    REQUIRE(basis.update_count()==0);
    REQUIRE(basis.refactorizations()==2);
    REQUIRE_THROWS_AS(basis.replace(0,3),NumericalError);
}
