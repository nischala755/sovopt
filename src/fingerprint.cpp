#include <sovereign/fingerprint.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace sovereign {
namespace {
class StableHash {
public:
    void byte(std::uint8_t b) { value_ ^= b; value_ *= 1099511628211ULL; }
    void u64(std::uint64_t v) { for (unsigned shift = 0; shift < 64; shift += 8) byte(static_cast<std::uint8_t>(v >> shift)); }
    void number(double v) { u64(std::bit_cast<std::uint64_t>(v)); }
    void text(std::string_view v) { u64(v.size()); for (unsigned char c : v) byte(c); }
    std::uint64_t value() const { return value_; }
private:
    std::uint64_t value_ = 14695981039346656037ULL;
};
}

ModelFingerprint fingerprint(const Model& m) {
    ModelFingerprint f;
    f.variables = m.variables.size(); f.constraints = m.constraints.size(); f.nonzeros = m.matrix.nonzeros();
    f.minimum_column_nonzeros = f.variables ? std::numeric_limits<Index>::max() : 0;
    f.minimum_row_nonzeros = f.constraints ? std::numeric_limits<Index>::max() : 0;
    std::vector<Index> row_counts(f.constraints, 0);
    long double sum_abs = 0, sum_squares = 0;
    StableHash hash;
    hash.text(m.name); hash.u64(m.variables.size()); hash.u64(m.constraints.size());
    hash.u64(static_cast<std::uint64_t>(m.sense)); hash.number(m.objective_offset);
    for (const auto& v : m.variables) {
        hash.text(v.name); hash.number(v.lower); hash.number(v.upper); hash.u64(static_cast<std::uint64_t>(v.type));
        if (v.type == VariableType::continuous) ++f.continuous_variables;
        else { ++f.integer_variables; if (v.type == VariableType::binary) ++f.binary_variables; }
        f.finite_variable_lowers += std::isfinite(v.lower); f.finite_variable_uppers += std::isfinite(v.upper);
        f.estimated_bytes += v.name.size() + sizeof(Variable);
    }
    for (const auto& r : m.constraints) {
        hash.text(r.name); hash.number(r.lower); hash.number(r.upper);
        f.finite_row_lowers += std::isfinite(r.lower); f.finite_row_uppers += std::isfinite(r.upper);
        if (std::isfinite(r.lower) && r.lower == r.upper) ++f.equality_rows;
        else if (std::isfinite(r.lower) && std::isfinite(r.upper)) ++f.ranged_rows;
        f.estimated_bytes += r.name.size() + sizeof(Constraint);
    }
    hash.u64(m.objective.size());
    for (double c : m.objective) { hash.number(c); f.objective_nonzeros += c != 0; }
    hash.u64(m.matrix.rows()); hash.u64(m.matrix.columns()); hash.u64(m.matrix.nonzeros());
    for (Index j = 0; j < m.matrix.columns(); ++j) {
        const auto column = m.matrix.column(j);
        f.minimum_column_nonzeros = std::min(f.minimum_column_nonzeros, column.values.size());
        f.maximum_column_nonzeros = std::max(f.maximum_column_nonzeros, column.values.size());
        hash.u64(column.values.size());
        for (Index k = 0; k < column.values.size(); ++k) {
            const double a = column.values[k], magnitude = std::abs(a);
            hash.u64(column.rows[k]); hash.number(a); ++row_counts[column.rows[k]];
            if (f.minimum_absolute_coefficient == 0 || magnitude < f.minimum_absolute_coefficient) f.minimum_absolute_coefficient = magnitude;
            f.maximum_absolute_coefficient = std::max(f.maximum_absolute_coefficient, magnitude);
            sum_abs += magnitude; sum_squares += static_cast<long double>(a) * a;
        }
    }
    for (Index count : row_counts) { f.minimum_row_nonzeros = std::min(f.minimum_row_nonzeros, count); f.maximum_row_nonzeros = std::max(f.maximum_row_nonzeros, count); }
    if (f.variables && f.constraints) f.density = static_cast<double>(f.nonzeros) / f.variables / f.constraints;
    if (f.nonzeros) {
        f.mean_absolute_coefficient = static_cast<double>(sum_abs / f.nonzeros);
        f.coefficient_l2_norm = std::sqrt(static_cast<double>(sum_squares));
    }
    if (f.variables) f.mean_column_nonzeros = static_cast<double>(f.nonzeros) / f.variables;
    if (f.constraints) f.mean_row_nonzeros = static_cast<double>(f.nonzeros) / f.constraints;
    f.estimated_bytes += sizeof(Model) + m.name.size() + m.objective.size() * sizeof(double) +
        (m.matrix.columns() + 1) * sizeof(Index) + m.matrix.nonzeros() * (sizeof(Index) + sizeof(double));
    f.stable_hash = hash.value();
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(16) << f.stable_hash; f.hash_hex = out.str();
    return f;
}
}
