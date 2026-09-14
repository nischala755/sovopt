#include "lp_standard.hpp"
#include <sovereign/basis.hpp>
#include <cmath>
#include <algorithm>

namespace sovereign::detail {
namespace {
double checked(double value) { if (!std::isfinite(value)) throw NumericalError("standard form arithmetic overflow"); return value; }
struct Row { std::vector<std::pair<Index,double>> entries; double rhs; char sense; RowOrigin origin; };
}
StandardForm standardize(const Model& m,bool scaling) {
    StandardForm f; f.variables.resize(m.variables.size());
    const double sense=m.sense==ObjectiveSense::minimize ? 1.0 : -1.0;
    std::vector<Row> rows;
    for (Index j=0;j<m.variables.size();++j) {
        const auto& v=m.variables[j]; auto& t=f.variables[j];
        const auto term=[&](double coefficient) { t.terms.emplace_back(f.cost.size(),coefficient); f.cost.push_back(checked(sense*m.objective[j]*coefficient)); };
        if (v.lower==v.upper) t.shift=v.lower;
        else if (std::isfinite(v.lower)) {
            t.shift=v.lower; term(1);
            if (std::isfinite(v.upper)) rows.push_back({{{t.terms[0].first,1}},checked(v.upper-v.lower),'L',{}});
        } else if (std::isfinite(v.upper)) { t.shift=v.upper; term(-1); }
        else { term(1); term(-1); }
    }
    std::vector<std::vector<std::pair<Index,double>>> original_rows(m.constraints.size());
    std::vector<double> shifts(m.constraints.size(),0);
    for (Index j=0;j<m.variables.size();++j) {
        const auto column=m.matrix.column(j);
        for (Index k=0;k<column.rows.size();++k) {
            const auto i=column.rows[k]; const auto a=column.values[k];
            shifts[i]=checked(std::fma(a,f.variables[j].shift,shifts[i]));
            for (auto [col,coefficient] : f.variables[j].terms) original_rows[i].emplace_back(col,checked(a*coefficient));
        }
    }
    for (Index i=0;i<m.constraints.size();++i) {
        const auto& r=m.constraints[i];
        if (r.lower==r.upper) rows.push_back({original_rows[i],checked(r.lower-shifts[i]),'E',{i,1,true}});
        else {
            if (std::isfinite(r.lower)) rows.push_back({original_rows[i],checked(r.lower-shifts[i]),'G',{i,1,true}});
            if (std::isfinite(r.upper)) rows.push_back({original_rows[i],checked(r.upper-shifts[i]),'L',{i,1,true}});
        }
    }
    for (auto& r : rows) {
        double factor=1;
        if (scaling) {
            double maximum=0;
            for (auto [j,a] : r.entries) { (void)j; maximum=std::max(maximum,std::abs(a)); }
            if (maximum>0) factor=checked(1/maximum);
        }
        if (r.rhs<0) {
            factor=-factor;
            if (r.sense=='L') r.sense='G'; else if (r.sense=='G') r.sense='L';
        }
        r.rhs=checked(r.rhs*factor); r.origin.factor*=factor;
        for (auto& [j,a] : r.entries) { (void)j; a=checked(a*factor); }
    }
    if (scaling) {
        std::vector<double> largest(f.cost.size(),0);
        for (const auto& r : rows) for (auto [j,a] : r.entries) largest[j]=std::max(largest[j],std::abs(a));
        std::vector<double> scale(f.cost.size(),1);
        for (Index j=0;j<scale.size();++j) if (largest[j]>0) { scale[j]=checked(1/largest[j]); f.cost[j]=checked(f.cost[j]*scale[j]); }
        for (auto& r : rows) for (auto& [j,a] : r.entries) a=checked(a*scale[j]);
        for (auto& t : f.variables) for (auto& [j,a] : t.terms) a=checked(a*scale[j]);
    }
    std::vector<Triplet> entries;
    for (Index i=0;i<rows.size();++i) {
        const auto& r=rows[i]; f.rhs.push_back(r.rhs); f.origins.push_back(r.origin);
        for (auto [j,a] : r.entries) entries.push_back({i,j,a});
        if (r.sense!='E') { entries.push_back({i,f.cost.size(),r.sense=='L' ? 1.0 : -1.0}); f.cost.push_back(0); }
    }
    f.artificial.assign(f.cost.size(),false);
    for (Index i=0;i<rows.size();++i) {
        f.basis.push_back(f.cost.size()); entries.push_back({i,f.cost.size(),1}); f.cost.push_back(0); f.artificial.push_back(true);
    }
    f.matrix=CscMatrix::from_triplets(rows.size(),f.cost.size(),std::move(entries));
    return f;
}
std::vector<double> StandardForm::restore(std::span<const double> point,bool direction) const {
    if (point.size()!=cost.size()) throw NumericalError("standard point dimension mismatch");
    std::vector<double> result; result.reserve(variables.size());
    for (const auto& t : variables) {
        double value=direction ? 0 : t.shift;
        for (auto [j,a] : t.terms) value=checked(std::fma(a,point[j],value));
        result.push_back(value);
    }
    return result;
}
void StandardForm::remove_row(Index row) {
    std::vector<Triplet> entries;
    for (Index j=0;j<matrix.columns();++j) {
        const auto col=matrix.column(j);
        for (Index k=0;k<col.rows.size();++k) if (col.rows[k]!=row) entries.push_back({col.rows[k]-(col.rows[k]>row ? 1 : 0),j,col.values[k]});
    }
    matrix=CscMatrix::from_triplets(matrix.rows()-1,matrix.columns(),std::move(entries));
    rhs.erase(rhs.begin()+static_cast<std::ptrdiff_t>(row)); origins.erase(origins.begin()+static_cast<std::ptrdiff_t>(row));
}
}
