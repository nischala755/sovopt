#pragma once
#include <sovereign/model.hpp>
namespace sovereign {
// Tightens row bounds only when the row activity is necessarily integral.
// Returns the number of strengthened sides. Matrix coefficients are unchanged.
Index apply_integer_row_cuts(Model&);
struct CutStatistics { Index integer_rounding=0, chvatal_gomory=0, cover=0, clique=0; };
// Adds globally valid structural cuts and, when possible, relaxation-separated cuts.
// When an LP point is supplied, additionally separates violated single-row
// Chvatal-Gomory cuts. Without a point only structural cuts are generated.
[[nodiscard]] CutStatistics apply_safe_root_cuts(Model&, std::span<const double> lp_point = {});
}
