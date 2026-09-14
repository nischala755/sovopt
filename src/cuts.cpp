#include <sovereign/cuts.hpp>
#include <cmath>
namespace sovereign {
Index apply_integer_row_cuts(Model& m) {
    std::vector<bool> eligible(m.constraints.size(),true), nonempty(m.constraints.size(),false);
    constexpr double exact_limit=9007199254740992.0;
    for(Index j=0;j<m.variables.size();++j) {
        const auto c=m.matrix.column(j);
        for(Index k=0;k<c.rows.size();++k) {
            const auto i=c.rows[k]; const double a=c.values[k]; nonempty[i]=true;
            if(m.variables[j].type==VariableType::continuous || !std::isfinite(a) || std::abs(a)>exact_limit || a!=std::trunc(a)) eligible[i]=false;
        }
    }
    Index count=0;
    for(Index i=0;i<m.constraints.size();++i) if(eligible[i]&&nonempty[i]) {
        auto& r=m.constraints[i];
        // Moving one ULP outward avoids strengthening an accidentally rounded
        // floating-point boundary. At huge bounds there is no useful fraction.
        if(std::isfinite(r.upper)&&std::abs(r.upper)<exact_limit) {
            const double b=std::floor(std::nextafter(r.upper,infinity));
            if(b<r.upper) { r.upper=b; ++count; }
        }
        if(std::isfinite(r.lower)&&std::abs(r.lower)<exact_limit) {
            const double b=std::ceil(std::nextafter(r.lower,-infinity));
            if(b>r.lower) { r.lower=b; ++count; }
        }
    }
    return count;
}
}
