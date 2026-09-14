#include <sovereign/cuts.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>
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

CutStatistics apply_safe_root_cuts(Model&m){
 CutStatistics stats;stats.integer_rounding=apply_integer_row_cuts(m);const Index original_rows=m.constraints.size();std::set<std::string>names;for(const auto&r:m.constraints)names.insert(r.name);std::vector<Triplet>entries;entries.reserve(m.matrix.nonzeros());std::vector<std::vector<std::pair<double,Index>>> row_terms(original_rows);std::vector<bool>eligible(original_rows,true);for(Index j=0;j<m.matrix.columns();++j){const auto c=m.matrix.column(j);for(Index k=0;k<c.rows.size();++k){entries.push_back({c.rows[k],j,c.values[k]});row_terms[c.rows[k]].push_back({c.values[k],j});if(m.variables[j].type!=VariableType::binary||m.variables[j].lower<0||c.values[k]<=0)eligible[c.rows[k]]=false;}}
 for(Index i=0;i<original_rows;++i){const auto&row=m.constraints[i];if(row.name.starts_with("cover_")||row.name.starts_with("clique_"))continue;if(!eligible[i]||!std::isfinite(row.upper)||row.upper<0||row_terms[i].size()<2)continue;auto terms=std::move(row_terms[i]);
  std::sort(terms.begin(),terms.end(),[](const auto&a,const auto&b){return a.first!=b.first?a.first>b.first:a.second<b.second;});double sum=0;Index cover_size=0;while(cover_size<terms.size()&&sum<=row.upper){sum+=terms[cover_size].first;++cover_size;}const auto cover_name="cover_"+std::to_string(i);if(sum>row.upper&&cover_size>=2&&!names.contains(cover_name)){const Index r=m.constraints.size();m.constraints.push_back({cover_name,-infinity,static_cast<double>(cover_size-1)});names.insert(cover_name);for(Index k=0;k<cover_size;++k)entries.push_back({r,terms[k].second,1});++stats.cover;}
  for(Index a=0;a<terms.size();++a)for(Index b=a+1;b<terms.size();++b)if(terms[a].first+terms[b].first>row.upper){const auto name="clique_"+std::to_string(i)+"_"+std::to_string(a)+"_"+std::to_string(b);if(names.contains(name))continue;const Index r=m.constraints.size();m.constraints.push_back({name,-infinity,1});names.insert(name);entries.push_back({r,terms[a].second,1});entries.push_back({r,terms[b].second,1});++stats.clique;}
 }
 if(m.constraints.size()!=original_rows)m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(entries));return stats;
}
}
