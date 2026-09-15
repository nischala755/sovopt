#include <sovereign/solution.hpp>
#include <cmath>
#include <stdexcept>
namespace sovereign {
std::string_view status_name(SolveStatus s) {
    switch(s) {
    case SolveStatus::optimal: return "optimal";
    case SolveStatus::infeasible: return "infeasible";
    case SolveStatus::unbounded: return "unbounded";
    case SolveStatus::time_limit: return "time_limit";
    case SolveStatus::node_limit: return "node_limit";
    case SolveStatus::iteration_limit: return "iteration_limit";
    case SolveStatus::numerical_failure: return "numerical_failure";
    case SolveStatus::cancelled: return "cancelled";
    }
    return "unknown";
}
void validate_options(const SolverOptions& o) {
    for(double t : {o.tolerances.primal,o.tolerances.dual,o.tolerances.integrality,o.tolerances.pivot})
        if(!std::isfinite(t)||t<=0||t>=1) throw std::invalid_argument("Tolerances must be finite and strictly between zero and one");
    if(!std::isfinite(o.time_limit_seconds)||o.time_limit_seconds<0) throw std::invalid_argument("Invalid time limit");
    if(!std::isfinite(o.mip_gap)||o.mip_gap<0) throw std::invalid_argument("Invalid MIP gap");
    if(o.branching!="most_fractional"&&o.branching!="pseudocost"&&o.branching!="strong") throw std::invalid_argument("Unsupported branching strategy");
    if(o.strong_branching_candidates==0) throw std::invalid_argument("Strong branching candidate count must be positive");
    if(o.feasibility_pump_passes==0) throw std::invalid_argument("Feasibility pump pass count must be positive");
    if(o.method!="auto"&&o.method!="simplex"&&o.method!="interior_point") throw std::invalid_argument("Unsupported continuous solver method");
}
}
