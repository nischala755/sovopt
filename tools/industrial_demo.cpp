#include <sovereign/generators.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mip.hpp>
#include <sovereign/validation.hpp>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <stdexcept>
#include <vector>

using namespace sovereign;
namespace {
struct Case { std::string_view domain; Model model; };
bool integer(const Model& model) { for(const auto& v:model.variables) if(v.type!=VariableType::continuous)return true;return false; }
void json_string(std::string_view value) { std::cout<<'"';for(char c:value){if(c=='"'||c=='\\')std::cout<<'\\';std::cout<<c;}std::cout<<'"'; }
}
int main(int argc,char** argv) {
    try {
        const Index scale=argc>1?static_cast<Index>(std::stoull(argv[1])):1;
        if(!scale||scale>1000)throw std::invalid_argument("scale must be between 1 and 1000");
        std::vector<Case> cases;
        cases.push_back({"refinery",generate_refinery_model(101,8*scale)});
        cases.push_back({"power_dispatch",generate_power_model(102,12*scale)});
        cases.push_back({"logistics",generate_logistics_model(103,6*scale)});
        cases.push_back({"crude_blending",generate_crude_blending_model(104,6*scale)});
        cases.push_back({"production_planning",generate_production_planning_model(105,3*scale,4)});
        cases.push_back({"supply_chain",generate_supply_chain_model(106,4*scale,8*scale)});
        cases.push_back({"process_optimization",generate_process_model(107,8*scale)});
        SolverOptions options;options.time_limit_seconds=30;options.node_limit=100000;options.iteration_limit=200000;
        bool all_verified=true;Index total_variables=0,total_rows=0,total_nonzeros=0;
        std::cout<<std::setprecision(17)<<"{\"schema_version\":1,\"evidence_class\":\"synthetic_industrial_demonstrator\",\"scale\":"<<scale<<",\"cases\":[";
        for(Index i=0;i<cases.size();++i){auto& item=cases[i];if(i)std::cout<<',';const auto validation=validate(item.model);SolveResult result;
            if(validation.ok())result=integer(item.model)?solve_mip(item.model,options):solve_lp(item.model,options);
            const bool verified=validation.ok()&&result.status==SolveStatus::optimal&&result.verification.passed;all_verified=all_verified&&verified;
            total_variables+=item.model.variables.size();total_rows+=item.model.constraints.size();total_nonzeros+=item.model.matrix.nonzeros();
            std::cout<<"{\"domain\":";json_string(item.domain);std::cout<<",\"model\":";json_string(item.model.name);
            std::cout<<",\"variables\":"<<item.model.variables.size()<<",\"rows\":"<<item.model.constraints.size()<<",\"nonzeros\":"<<item.model.matrix.nonzeros()<<",\"integer\":"<<(integer(item.model)?"true":"false")<<",\"status\":";json_string(status_name(result.status));
            std::cout<<",\"verified\":"<<(verified?"true":"false")<<",\"objective\":";
            if(std::isfinite(result.objective))std::cout<<result.objective;else std::cout<<"null";
            std::cout<<",\"iterations\":"<<result.iterations<<",\"nodes\":"<<result.nodes<<",\"runtime_seconds\":"<<result.runtime_seconds<<'}';
        }
        std::cout<<"],\"totals\":{\"variables\":"<<total_variables<<",\"rows\":"<<total_rows<<",\"nonzeros\":"<<total_nonzeros<<"},\"all_verified\":"<<(all_verified?"true":"false")<<"}\n";
        return all_verified?0:2;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 3;}
}
