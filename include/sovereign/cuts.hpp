#pragma once
#include <sovereign/model.hpp>
namespace sovereign {
// Tightens row bounds only when the row activity is necessarily integral.
// Returns the number of strengthened sides. Matrix coefficients are unchanged.
Index apply_integer_row_cuts(Model&);
}
