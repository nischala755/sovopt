#pragma once
#include <sovereign/sparse_matrix.hpp>
#include <map>
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
}
