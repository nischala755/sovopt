#include <sovereign/cuts.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>
#include <sstream>
#include <iomanip>
namespace sovereign {
namespace {
void append_single_row_cg(Model& m,Index original_rows,
                          const std::vector<std::vector<std::pair<double,Index>>>& row_terms,
                          std::vector<Triplet>& entries,std::set<std::string>& names,
                          CutStatistics& stats,std::span<const double> lp_point) {
    if(lp_point.size()!=m.variables.size()) return;
    constexpr long double exact_limit=9007199254740992.0L;
    for(Index row_index=0;row_index<original_rows;++row_index) {
        const auto& terms=row_terms[row_index];
        if(terms.empty()) continue;
        bool eligible=true;
        for(const auto& [coefficient,column]:terms) {
            const auto& variable=m.variables[column];
            if(variable.type==VariableType::continuous||!std::isfinite(variable.lower)||
               variable.lower!=std::trunc(variable.lower)||std::abs(variable.lower)>=exact_limit||
               !std::isfinite(coefficient)) eligible=false;
        }
        if(!eligible) continue;
        std::vector<double> multipliers{1.0};
        for(const auto& [coefficient,column]:terms) {
            (void)column;
            if(coefficient==0) continue;
            const double candidate=std::nextafter(1.0/std::abs(coefficient),infinity);
            if(std::isfinite(candidate)&&candidate>0&&candidate<=1e6&&
               std::find(multipliers.begin(),multipliers.end(),candidate)==multipliers.end())
                multipliers.push_back(candidate);
            if(multipliers.size()>=9) break;
        }
        std::set<std::string> signatures;
        const auto generate=[&](double side_sign,double bound,const char* side) {
            if(!std::isfinite(bound)) return;
            for(Index multiplier_index=0;multiplier_index<multipliers.size();++multiplier_index) {
                const long double multiplier=multipliers[multiplier_index];
                long double shifted=side_sign*static_cast<long double>(bound);
                for(const auto& [coefficient,column]:terms)
                    shifted-=side_sign*static_cast<long double>(coefficient)*m.variables[column].lower;
                const long double scaled_rhs=multiplier*shifted;
                if(!std::isfinite(static_cast<double>(scaled_rhs))||std::abs(scaled_rhs)>=exact_limit) continue;
                const long double rounded_rhs=std::floor(scaled_rhs);
                std::vector<std::pair<double,Index>> cut;
                long double translated_rhs=rounded_rhs;
                bool safe=true;
                for(const auto& [coefficient,column]:terms) {
                    const long double scaled=multiplier*side_sign*static_cast<long double>(coefficient);
                    if(std::abs(scaled)>=exact_limit) { safe=false; break; }
                    const long double rounded=std::floor(scaled);
                    if(rounded!=0) cut.push_back({static_cast<double>(rounded),column});
                    translated_rhs+=rounded*m.variables[column].lower;
                }
                if(!safe||cut.empty()||std::abs(translated_rhs)>=exact_limit) continue;
                bool identical=translated_rhs==side_sign*static_cast<long double>(bound);
                Index nonzero_original=0;
                for(const auto& [coefficient,column]:terms) {
                    if(coefficient==0) continue;
                    ++nonzero_original;
                    const auto found=std::find_if(cut.begin(),cut.end(),[&](const auto& value){return value.second==column;});
                    if(found==cut.end()||found->first!=side_sign*coefficient) identical=false;
                }
                if(identical&&cut.size()==nonzero_original) continue;
                long double activity=0;
                for(const auto& [coefficient,column]:cut)
                    activity+=static_cast<long double>(coefficient)*lp_point[column];
                const long double efficacy=activity-translated_rhs;
                if(!(efficacy>1e-9L*(1+std::abs(translated_rhs)))) continue;
                std::ostringstream signature;
                signature<<std::setprecision(17)<<static_cast<double>(translated_rhs)<<':';
                for(const auto& [coefficient,column]:cut) signature<<column<<'='<<coefficient<<',';
                if(!signatures.insert(signature.str()).second) continue;
                const auto name="cg_"+std::to_string(row_index)+"_"+side+"_"+std::to_string(multiplier_index);
                if(names.contains(name)) continue;
                const Index new_row=m.constraints.size();
                m.constraints.push_back({name,-infinity,static_cast<double>(translated_rhs)});
                names.insert(name);
                for(const auto& [coefficient,column]:cut) entries.push_back({new_row,column,coefficient});
                ++stats.chvatal_gomory;
            }
        };
        generate(1,m.constraints[row_index].upper,"upper");
        generate(-1,m.constraints[row_index].lower,"lower");
    }
}
}
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

CutStatistics apply_safe_root_cuts(Model&m,std::span<const double> lp_point){
 CutStatistics stats;stats.integer_rounding=apply_integer_row_cuts(m);const Index original_rows=m.constraints.size();std::set<std::string>names;for(const auto&r:m.constraints)names.insert(r.name);std::vector<Triplet>entries;entries.reserve(m.matrix.nonzeros());std::vector<std::vector<std::pair<double,Index>>> row_terms(original_rows);std::vector<bool>eligible(original_rows,true);for(Index j=0;j<m.matrix.columns();++j){const auto c=m.matrix.column(j);for(Index k=0;k<c.rows.size();++k){entries.push_back({c.rows[k],j,c.values[k]});row_terms[c.rows[k]].push_back({c.values[k],j});if(m.variables[j].type!=VariableType::binary||m.variables[j].lower<0||c.values[k]<=0)eligible[c.rows[k]]=false;}}
 append_single_row_cg(m,original_rows,row_terms,entries,names,stats,lp_point);
 for(Index i=0;i<original_rows;++i){const auto&row=m.constraints[i];if(row.name.starts_with("cover_")||row.name.starts_with("clique_"))continue;if(!eligible[i]||!std::isfinite(row.upper)||row.upper<0||row_terms[i].size()<2)continue;auto terms=std::move(row_terms[i]);
  std::sort(terms.begin(),terms.end(),[](const auto&a,const auto&b){return a.first!=b.first?a.first>b.first:a.second<b.second;});double sum=0;Index cover_size=0;while(cover_size<terms.size()&&sum<=row.upper){sum+=terms[cover_size].first;++cover_size;}const auto cover_name="cover_"+std::to_string(i);if(sum>row.upper&&cover_size>=2&&!names.contains(cover_name)){const Index r=m.constraints.size();m.constraints.push_back({cover_name,-infinity,static_cast<double>(cover_size-1)});names.insert(cover_name);for(Index k=0;k<cover_size;++k)entries.push_back({r,terms[k].second,1});++stats.cover;}
  for(Index a=0;a<terms.size();++a)for(Index b=a+1;b<terms.size();++b)if(terms[a].first+terms[b].first>row.upper){const auto name="clique_"+std::to_string(i)+"_"+std::to_string(a)+"_"+std::to_string(b);if(names.contains(name))continue;const Index r=m.constraints.size();m.constraints.push_back({name,-infinity,1});names.insert(name);entries.push_back({r,terms[a].second,1});entries.push_back({r,terms[b].second,1});++stats.clique;}
 }
 if(m.constraints.size()!=original_rows)m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(entries));return stats;
}

Index apply_tableau_gmi_cuts(Model& m,const LpTableau& tableau,
                             std::span<const double> lp_point,double efficacy_tolerance,Index limit) {
 if(lp_point.size()!=m.variables.size()||tableau.columns.empty()||limit==0||
    !std::isfinite(efficacy_tolerance)||efficacy_tolerance<=0) return 0;
 std::vector<bool> basic(tableau.columns.size(),false);for(const auto&row:tableau.rows)if(row.basic_column<basic.size())basic[row.basic_column]=true;
 std::vector<Triplet> entries;entries.reserve(m.matrix.nonzeros());for(Index j=0;j<m.matrix.columns();++j){const auto c=m.matrix.column(j);for(Index k=0;k<c.rows.size();++k)entries.push_back({c.rows[k],j,c.values[k]});}
 std::set<std::string> names;for(const auto&r:m.constraints)names.insert(r.name);Index added=0;
 for(Index row_index=0;row_index<tableau.rows.size()&&added<limit;++row_index){const auto&row=tableau.rows[row_index];if(row.basic_column>=tableau.columns.size()||!tableau.columns[row.basic_column].integer_lattice||row.coefficients.size()!=tableau.columns.size())continue;
  const double f0=row.rhs-std::floor(row.rhs);if(!(f0>1e-9&&f0<1-1e-9))continue;std::vector<long double> coefficients(m.variables.size());long double constant=0;bool safe=true;
  for(Index j=0;j<tableau.columns.size();++j){if(basic[j]||tableau.columns[j].artificial)continue;const double a=row.coefficients[j];if(std::abs(a)<=1e-14)continue;double gamma=0;
   if(tableau.columns[j].integer_lattice){const double fraction=a-std::floor(a);gamma=fraction<=f0?fraction/f0:(1-fraction)/(1-f0);}
   else gamma=a>=0?a/f0:-a/(1-f0);
   if(!std::isfinite(gamma)||gamma<0){safe=false;break;}if(gamma<=1e-14)continue;const auto&column=tableau.columns[j];if(!column.representable){safe=false;break;}constant+=static_cast<long double>(gamma)*column.expression_constant;for(const auto&[original,value]:column.original_expression)coefficients[original]+=static_cast<long double>(gamma)*value;
  }
  if(!safe)continue;const long double lower=1-constant;long double activity=0;for(Index j=0;j<coefficients.size();++j)activity+=coefficients[j]*lp_point[j];if(!(lower-activity>efficacy_tolerance*(1+std::abs(lower))))continue;
  const auto name="gmi_"+std::to_string(row_index);if(names.contains(name))continue;const Index new_row=m.constraints.size();m.constraints.push_back({name,static_cast<double>(lower),infinity});names.insert(name);for(Index j=0;j<coefficients.size();++j)if(coefficients[j]!=0)entries.push_back({new_row,j,static_cast<double>(coefficients[j])});++added;
 }
 if(added)m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(entries));return added;
}
}
