#include <sovereign/basis.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <limits>

namespace sovereign {
namespace {
double finite(double value) {
    if (!std::isfinite(value)) throw NumericalError("Sparse basis arithmetic produced a nonfinite value");
    return value;
}
}

SparseBasis::SparseBasis(const CscMatrix& matrix, std::span<const Index> basis_columns,
                         double pivot_tolerance) {
    if (!std::isfinite(pivot_tolerance) || pivot_tolerance <= 0)
        throw NumericalError("Basis pivot tolerance must be finite and positive");
    if (basis_columns.size() != matrix.rows())
        throw NumericalError("Basis must contain one column per matrix row");
    std::set<Index> selected;
    for (const Index column : basis_columns) {
        if (column >= matrix.columns()) throw NumericalError("Basis column is out of range");
        if (!selected.insert(column).second) throw NumericalError("Basis contains duplicate columns");
    }
    const Index n = matrix.rows();
    std::vector<Triplet> basis_entries;
    for (Index j = 0; j < n; ++j) {
        const auto column = matrix.column(basis_columns[j]);
        for (Index k = 0; k < column.rows.size(); ++k)
            basis_entries.push_back({column.rows[k], j, column.values[k]});
    }
    basis_matrix_ = CscMatrix::from_triplets(n, n, std::move(basis_entries));
    factors_.resize(n);
    permutation_.resize(n);
    std::iota(permutation_.begin(), permutation_.end(), Index{0});
    for (Index j = 0; j < n; ++j) {
        const auto column = matrix.column(basis_columns[j]);
        for (Index k = 0; k < column.rows.size(); ++k)
            factors_[column.rows[k]].emplace(j, finite(column.values[k]));
    }
    for (Index k = 0; k < n; ++k) {
        Index pivot_row = k;
        double largest = 0;
        for (Index i = k; i < n; ++i) {
            const auto entry = factors_[i].find(k);
            if (entry != factors_[i].end() && std::abs(entry->second) > largest) {
                largest = std::abs(entry->second);
                pivot_row = i;
            }
        }
        if (largest <= pivot_tolerance) throw NumericalError("Singular or numerically small basis pivot");
        if (pivot_row != k) {
            std::swap(factors_[pivot_row], factors_[k]);
            std::swap(permutation_[pivot_row], permutation_[k]);
        }
        const double pivot = factors_[k].at(k);
        for (Index i = k + 1; i < n; ++i) {
            auto entry = factors_[i].find(k);
            if (entry == factors_[i].end()) continue;
            const double multiplier = finite(entry->second / pivot);
            entry->second = multiplier;
            for (auto upper = factors_[k].upper_bound(k); upper != factors_[k].end(); ++upper) {
                auto [target, inserted] = factors_[i].try_emplace(upper->first, 0.0);
                (void)inserted;
                target->second = finite(target->second - finite(multiplier * upper->second));
                // Only exact cancellation is removed: small fill can affect later pivots.
                if (target->second == 0) factors_[i].erase(target);
            }
            if (multiplier == 0) factors_[i].erase(k);
        }
    }
}

void SparseBasis::validate_rhs(std::span<const double> rhs) const {
    if (rhs.size() != dimension()) throw NumericalError("Basis right-hand side has incorrect dimension");
    for (const double value : rhs) (void)finite(value);
}

std::vector<double> SparseBasis::solve_factors(std::span<const double> rhs) const {
    const Index n = dimension();
    std::vector<double> result(n);
    for (Index i = 0; i < n; ++i) {
        double value = rhs[permutation_[i]];
        for (auto entry = factors_[i].begin(); entry != factors_[i].lower_bound(i); ++entry)
            value = finite(value - finite(entry->second * result[entry->first]));
        result[i] = value;
    }
    for (Index i = n; i-- > 0;) {
        double value = result[i];
        for (auto entry = factors_[i].upper_bound(i); entry != factors_[i].end(); ++entry)
            value = finite(value - finite(entry->second * result[entry->first]));
        result[i] = finite(value / factors_[i].at(i));
    }
    return result;
}

std::vector<double> SparseBasis::solve_transpose_factors(std::span<const double> rhs) const {
    const Index n = dimension();
    std::vector<double> work(rhs.begin(), rhs.end());
    // U^T y = rhs, then L^T z = y, then x = P^T z.
    for (Index i = 0; i < n; ++i) {
        work[i] = finite(work[i] / factors_[i].at(i));
        for (auto entry = factors_[i].upper_bound(i); entry != factors_[i].end(); ++entry)
            work[entry->first] = finite(work[entry->first] - finite(entry->second * work[i]));
    }
    for (Index i = n; i-- > 0;) {
        for (auto entry = factors_[i].begin(); entry != factors_[i].lower_bound(i); ++entry)
            work[entry->first] = finite(work[entry->first] - finite(entry->second * work[i]));
    }
    std::vector<double> result(n);
    for (Index i = 0; i < n; ++i) result[permutation_[i]] = work[i];
    return result;
}

namespace {
double infinity_norm(std::span<const double> values) {
    double result = 0;
    for (double value : values) result = std::max(result, std::abs(value));
    return result;
}
double matrix_infinity_norm(const CscMatrix& matrix, bool transpose) {
    std::vector<double> sums(transpose ? matrix.columns() : matrix.rows(), 0);
    for (Index j=0;j<matrix.columns();++j) {
        const auto column=matrix.column(j);
        for (Index k=0;k<column.rows.size();++k)
            sums[transpose ? j : column.rows[k]] += std::abs(column.values[k]);
    }
    return infinity_norm(sums);
}
}

std::vector<double> SparseBasis::solve(std::span<const double> rhs) const {
    validate_rhs(rhs);
    auto result=solve_factors(rhs);
    last_solve_info_={};
    double previous=std::numeric_limits<double>::infinity();
    for (Index pass=0;pass<=3;++pass) {
        const auto product=basis_matrix_.multiply(result);
        std::vector<double> residual(rhs.size());
        for (Index i=0;i<rhs.size();++i) residual[i]=finite(rhs[i]-product[i]);
        const double absolute=infinity_norm(residual);
        const double scale=std::max(std::numeric_limits<double>::min(),
            infinity_norm(rhs)+matrix_infinity_norm(basis_matrix_,false)*infinity_norm(result));
        last_solve_info_.scaled_residual=absolute/scale;
        if (absolute==0 || pass==3 || !(absolute<previous)) break;
        previous=absolute;
        const auto correction=solve_factors(residual);
        for (Index i=0;i<result.size();++i) result[i]=finite(result[i]+correction[i]);
        ++last_solve_info_.refinements;
    }
    return result;
}

std::vector<double> SparseBasis::solve_transpose(std::span<const double> rhs) const {
    validate_rhs(rhs);
    auto result=solve_transpose_factors(rhs);
    last_solve_info_={};
    double previous=std::numeric_limits<double>::infinity();
    for (Index pass=0;pass<=3;++pass) {
        const auto product=basis_matrix_.transpose_multiply(result);
        std::vector<double> residual(rhs.size());
        for (Index i=0;i<rhs.size();++i) residual[i]=finite(rhs[i]-product[i]);
        const double absolute=infinity_norm(residual);
        const double scale=std::max(std::numeric_limits<double>::min(),
            infinity_norm(rhs)+matrix_infinity_norm(basis_matrix_,true)*infinity_norm(result));
        last_solve_info_.scaled_residual=absolute/scale;
        if (absolute==0 || pass==3 || !(absolute<previous)) break;
        previous=absolute;
        const auto correction=solve_transpose_factors(residual);
        for (Index i=0;i<result.size();++i) result[i]=finite(result[i]+correction[i]);
        ++last_solve_info_.refinements;
    }
    return result;
}

Index SparseBasis::nonzeros() const noexcept {
    Index count = 0;
    for (const auto& row : factors_) count += row.size();
    return count;
}
}
