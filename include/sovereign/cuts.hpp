#pragma once
#include <sovereign/model.hpp>
namespace sovereign {
// Tightens row bounds only when the row activity is necessarily integral.
// Returns the number of strengthened sides. Matrix coefficients are unchanged.
Index apply_integer_row_cuts(Model&);
struct CutStatistics { Index integer_rounding=0, cover=0, clique=0; };
// Adds globally valid cuts only for nonnegative, binary knapsack rows.
[[nodiscard]] CutStatistics apply_safe_root_cuts(Model&);
}
