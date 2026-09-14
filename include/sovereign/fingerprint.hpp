#pragma once
#include <sovereign/model.hpp>
#include <cstdint>

namespace sovereign {
struct ModelFingerprint {
    std::uint64_t stable_hash = 0;
    std::string hash_hex;
    Index variables = 0, constraints = 0, nonzeros = 0;
    Index continuous_variables = 0, integer_variables = 0, binary_variables = 0;
    Index finite_variable_lowers = 0, finite_variable_uppers = 0;
    Index finite_row_lowers = 0, finite_row_uppers = 0;
    Index equality_rows = 0, ranged_rows = 0, objective_nonzeros = 0;
    Index minimum_column_nonzeros = 0, maximum_column_nonzeros = 0;
    Index minimum_row_nonzeros = 0, maximum_row_nonzeros = 0;
    double density = 0;
    double minimum_absolute_coefficient = 0, maximum_absolute_coefficient = 0;
    double mean_absolute_coefficient = 0, coefficient_l2_norm = 0;
    double mean_column_nonzeros = 0, mean_row_nonzeros = 0;
    std::size_t estimated_bytes = 0;
};

// Hashes an endian-independent byte encoding of every logical Model field,
// including exact IEEE-754 bit patterns, ordering, names, and sparse entries.
[[nodiscard]] ModelFingerprint fingerprint(const Model& model);
}
