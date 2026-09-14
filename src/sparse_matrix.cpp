#include <sovereign/sparse_matrix.hpp>
#include <sovereign/errors.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace sovereign {
CscMatrix CscMatrix::from_triplets(Index rows, Index columns, std::vector<Triplet> entries) {
    if (columns == std::numeric_limits<Index>::max()) throw InvalidModelError("column count overflow");
    for (const auto& e : entries) {
        if (e.row >= rows || e.column >= columns) throw InvalidModelError("matrix coordinate out of bounds");
        if (!std::isfinite(e.value)) throw InvalidModelError("matrix coefficients must be finite");
    }
    // Stable ordering makes duplicate accumulation reproducible for a given input.
    std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return std::tie(a.column,a.row) < std::tie(b.column,b.row);
    });
    CscMatrix result;
    result.rows_ = rows; result.columns_ = columns;
    result.offsets_.assign(columns + 1, 0);
    for (Index i = 0; i < entries.size();) {
        const auto row = entries[i].row;
        const auto col = entries[i].column;
        double value = 0;
        do {
            value += entries[i++].value;
            if (!std::isfinite(value)) throw InvalidModelError("duplicate coefficient sum overflow");
        } while (i < entries.size() && entries[i].row == row && entries[i].column == col);
        if (value != 0) {
            result.row_indices_.push_back(row); result.values_.push_back(value);
            ++result.offsets_[col + 1];
        }
    }
    for (Index j = 0; j < columns; ++j) result.offsets_[j + 1] += result.offsets_[j];
    return result;
}
SparseColumn CscMatrix::column(Index index) const {
    if (index >= columns_) throw std::out_of_range("matrix column out of bounds");
    const auto begin = offsets_[index]; const auto size = offsets_[index + 1] - begin;
    return {std::span<const Index>(row_indices_).subspan(begin,size), std::span<const double>(values_).subspan(begin,size)};
}
std::vector<double> CscMatrix::multiply(std::span<const double> x) const {
    if (x.size() != columns_) throw InvalidModelError("matrix-vector dimension mismatch");
    std::vector<double> result(rows_,0);
    for (Index j = 0; j < columns_; ++j)
        for (Index k = offsets_[j]; k < offsets_[j+1]; ++k) result[row_indices_[k]] += values_[k] * x[j];
    return result;
}
std::vector<double> CscMatrix::transpose_multiply(std::span<const double> x) const {
    if (x.size() != rows_) throw InvalidModelError("transpose-vector dimension mismatch");
    std::vector<double> result(columns_,0);
    for (Index j = 0; j < columns_; ++j)
        for (Index k = offsets_[j]; k < offsets_[j+1]; ++k) result[j] += values_[k] * x[row_indices_[k]];
    return result;
}
}
