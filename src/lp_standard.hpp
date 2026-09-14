#pragma once
#include <sovereign/solution.hpp>

namespace sovereign::detail {
struct VariableTransform { double shift=0; std::vector<std::pair<Index,double>> terms; };
struct RowOrigin { Index row=0; double factor=0; bool original=false; };
struct StandardForm {
    CscMatrix matrix;
    std::vector<double> rhs,cost;
    std::vector<Index> basis;
    std::vector<bool> artificial;
    std::vector<VariableTransform> variables;
    std::vector<RowOrigin> origins;
    std::vector<double> restore(std::span<const double> point, bool direction=false) const;
    void remove_row(Index row);
};
StandardForm standardize(const Model& model,bool scaling);
}
