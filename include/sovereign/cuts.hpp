#pragma once
#include <sovereign/model.hpp>
#include <sovereign/lp.hpp>
namespace sovereign {
// Tightens row bounds only when the row activity is necessarily integral.
// Returns the number of strengthened sides. Matrix coefficients are unchanged.
Index apply_integer_row_cuts(Model&);
struct CutStatistics { Index integer_rounding=0, chvatal_gomory=0, gmi=0, cover=0, clique=0; };
// Adds globally valid structural cuts and, when possible, relaxation-separated cuts.
// When an LP point is supplied, additionally separates violated single-row
// Chvatal-Gomory cuts. Without a point only structural cuts are generated.
[[nodiscard]] CutStatistics apply_safe_root_cuts(Model&, std::span<const double> lp_point = {});
// Maps valid GMI inequalities from a solved standard-form tableau back into
// original model coordinates. Unrepresentable transformed columns are skipped.
[[nodiscard]] Index apply_tableau_gmi_cuts(Model&, const LpTableau&, std::span<const double> lp_point,
                                            double efficacy_tolerance = 1e-9, Index limit = 32);
}
