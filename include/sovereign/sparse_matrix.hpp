#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace sovereign {
using Index = std::size_t;
struct Triplet { Index row; Index column; double value; };
struct SparseColumn { std::span<const Index> rows; std::span<const double> values; };

// Canonical compressed sparse columns. No explicit zeros or duplicate coordinates.
class CscMatrix {
public:
    CscMatrix() = default;
    [[nodiscard]] static CscMatrix from_triplets(Index rows, Index columns, std::vector<Triplet> entries);
    [[nodiscard]] Index rows() const noexcept { return rows_; }
    [[nodiscard]] Index columns() const noexcept { return columns_; }
    [[nodiscard]] Index nonzeros() const noexcept { return values_.size(); }
    [[nodiscard]] std::span<const Index> column_offsets() const noexcept { return offsets_; }
    [[nodiscard]] SparseColumn column(Index index) const;
    [[nodiscard]] std::vector<double> multiply(std::span<const double> x) const;
    [[nodiscard]] std::vector<double> transpose_multiply(std::span<const double> x) const;
private:
    Index rows_ = 0;
    Index columns_ = 0;
    std::vector<Index> offsets_{0};
    std::vector<Index> row_indices_;
    std::vector<double> values_;
};
}
