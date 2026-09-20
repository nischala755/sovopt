#pragma once
#include <sovereign/sparse_matrix.hpp>
#include <map>
#include <memory>
#include <stdexcept>

namespace sovereign {
struct BasisSolveInfo {
    double scaled_residual = 0;
    Index refinements = 0;
};

class NumericalError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Sparse row LU with partial pivoting: P * B = L * U.
// Lower multipliers and upper coefficients share each row; L has implicit unit diagonal.
class SparseBasis {
public:
    SparseBasis(const CscMatrix& matrix, std::span<const Index> basis_columns,
                double pivot_tolerance = 1e-12);
    [[nodiscard]] std::vector<double> solve(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> solve_transpose(std::span<const double> rhs) const;
    [[nodiscard]] Index dimension() const noexcept { return factors_.size(); }
    [[nodiscard]] Index nonzeros() const noexcept;
    [[nodiscard]] BasisSolveInfo last_solve_info() const noexcept { return last_solve_info_; }
private:
    CscMatrix basis_matrix_;
    std::vector<std::map<Index, double>> factors_;
    std::vector<Index> permutation_;
    mutable BasisSolveInfo last_solve_info_;
    void validate_rhs(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> solve_factors(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> solve_transpose_factors(std::span<const double> rhs) const;
};

// Product-form inverse basis. A stable sparse LU factorization is retained as
// the base and accepted pivots are represented by eta matrices until the
// controlled refactorization limit is reached.
class UpdatedBasis {
public:
    UpdatedBasis(const CscMatrix& matrix, std::vector<Index> basis_columns,
                 double pivot_tolerance = 1e-12, Index update_limit = 32);
    [[nodiscard]] std::vector<double> solve(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> solve_transpose(std::span<const double> rhs) const;
    void replace(Index position, Index entering_column);
    [[nodiscard]] const std::vector<Index>& columns() const noexcept { return columns_; }
    [[nodiscard]] Index update_count() const noexcept { return etas_.size(); }
    [[nodiscard]] Index refactorizations() const noexcept { return refactorizations_; }
    [[nodiscard]] BasisSolveInfo last_solve_info() const noexcept { return last_solve_info_; }
private:
    struct Eta { Index position; std::vector<double> column; };
    const CscMatrix* matrix_;
    std::vector<Index> columns_;
    double pivot_tolerance_;
    Index update_limit_;
    Index refactorizations_ = 0;
    std::unique_ptr<SparseBasis> base_;
    std::vector<Eta> etas_;
    mutable BasisSolveInfo last_solve_info_;
    void refactorize();
    [[nodiscard]] std::vector<double> matrix_column(Index column) const;
    [[nodiscard]] std::vector<double> solve_raw(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> solve_transpose_raw(std::span<const double> rhs) const;
    [[nodiscard]] std::vector<double> multiply_current(std::span<const double> x,bool transpose) const;
};
}
