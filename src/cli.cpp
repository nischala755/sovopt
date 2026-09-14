#include <sovereign/cli.hpp>
#include <sovereign/config.hpp>
#include <sovereign/errors.hpp>
#include <sovereign/validation.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mip.hpp>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <charconv>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace sovereign {
namespace {
constexpr std::string_view usage =
    "Sovereign Optimizer 0.1.0 - model foundation (no solver)\n"
    "Usage: sovereign inspect|validate|solve MODEL.mps [--json] [--config FILE]\n"
    "       [--mps-format free|fixed]\n"
    "       solve options: [--method auto|simplex] [--branching most_fractional|pseudocost]\n"
    "       [--time-limit SEC] [--node-limit N] [--iteration-limit N] [--mip-gap GAP]\n"
    "       sovereign --help | --version\n";

void print_statistics(std::ostream& out, const Model& m, bool json) {
    const auto s = statistics(m);
    const auto sense = m.sense == ObjectiveSense::minimize ? "minimize" : "maximize";
    if (json) {
        out << "{\"name\":" << json_quote(m.name) << ",\"objective_sense\":" << json_quote(sense)
            << ",\"objective_offset\":" << m.objective_offset
            << ",\"variables\":" << s.variables << ",\"constraints\":" << s.constraints << ",\"nonzeros\":" << s.nonzeros
            << ",\"continuous_variables\":" << s.continuous_variables << ",\"integer_variables\":" << s.integer_variables
            << ",\"binary_variables\":" << s.binary_variables << ",\"equality_rows\":" << s.equality_rows
            << ",\"ranged_rows\":" << s.ranged_rows << ",\"objective_nonzeros\":" << s.objective_nonzeros
            << ",\"density\":" << s.density << ",\"minimum_absolute_coefficient\":" << s.minimum_absolute_coefficient
            << ",\"maximum_absolute_coefficient\":" << s.maximum_absolute_coefficient
            << ",\"validation\":\"passed\",\"scope\":\"structural\"}\n";
    } else {
        out << "Model: " << m.name << "\nObjective sense: " << sense << "\nObjective offset: " << m.objective_offset
            << "\nVariables: " << s.variables << "\nConstraints: " << s.constraints << "\nNonzeros: " << s.nonzeros
            << "\nContinuous variables: " << s.continuous_variables << "\nInteger variables (including binary): " << s.integer_variables
            << "\nBinary variables: " << s.binary_variables << "\nEquality rows: " << s.equality_rows << "\nRanged rows: " << s.ranged_rows
            << "\nObjective nonzeros: " << s.objective_nonzeros << "\nDensity: " << s.density
            << "\nMinimum absolute coefficient: " << s.minimum_absolute_coefficient << "\nMaximum absolute coefficient: " << s.maximum_absolute_coefficient
            << "\nStructural validation: PASSED\n";
    }
}
void print_solution(std::ostream& out,const Model& model,const SolveResult& r,bool json) {
    if(json) {
        out << "{\"status\":" << json_quote(status_name(r.status)) << ",\"message\":" << json_quote(r.message)
            << ",\"objective\":"; if(std::isfinite(r.objective)) out<<r.objective; else out<<"null";
        out << ",\"best_bound\":"; if(std::isfinite(r.best_bound)) out<<r.best_bound; else out<<"null";
        out << ",\"mip_gap\":"; if(std::isfinite(r.mip_gap)) out<<r.mip_gap; else out<<"null";
        out << ",\"iterations\":"<<r.iterations<<",\"nodes\":"<<r.nodes<<",\"runtime_seconds\":"<<r.runtime_seconds
            <<",\"verified\":"<<(r.verification.passed?"true":"false")<<",\"variables\":{";
        for(Index j=0;j<r.primal.size();++j) { if(j) out<<','; out<<json_quote(model.variables[j].name)<<':'<<r.primal[j]; }
        out<<"}}\n";
    } else {
        out<<"Status: "<<status_name(r.status)<<"\n";
        if(std::isfinite(r.objective)) out<<"Objective: "<<r.objective<<"\n";
        if(std::isfinite(r.best_bound)) out<<"Best bound: "<<r.best_bound<<"\n";
        if(std::isfinite(r.mip_gap)) out<<"MIP gap: "<<r.mip_gap<<"\n";
        out<<"Iterations: "<<r.iterations<<"\nNodes: "<<r.nodes<<"\nRuntime seconds: "<<r.runtime_seconds<<"\nVerified: "<<(r.verification.passed?"yes":"no")<<"\n";
        for(Index j=0;j<r.primal.size();++j) out<<model.variables[j].name<<" = "<<r.primal[j]<<"\n";
    }
}
}
int run_cli(std::span<const std::string_view> args, std::ostream& output, std::ostream& errors) {
    try {
        if (args.size() == 1 && args[0] == "--help") { output << usage; return output ? 0 : 4; }
        if (args.size() == 1 && args[0] == "--version") { output << "Sovereign Optimizer 0.1.0\n"; return output ? 0 : 4; }
        if (args.size() < 2 || (args[0] != "inspect" && args[0] != "validate" && args[0]!="solve")) throw ConfigurationError(std::string(usage));
        const auto path = args[1];
        if (path.starts_with("--")) throw ConfigurationError("expected model path");
        bool json = false;
        std::optional<std::string> config_path;
        std::optional<MpsFormat> format;
        std::optional<std::string> method,branching;
        std::optional<double> time_limit,gap;
        std::optional<Index> node_limit,iteration_limit;
        std::unordered_set<std::string_view> options;
        for (Index i = 2; i < args.size(); ++i) {
            const auto option = args[i];
            if (!options.insert(option).second) throw ConfigurationError("duplicate option: " + std::string(option));
            if (option == "--json") json = true;
            else if (option == "--config" || option == "--mps-format" || option=="--method" || option=="--branching" || option=="--time-limit" || option=="--mip-gap" || option=="--node-limit" || option=="--iteration-limit") {
                if (++i == args.size() || args[i].starts_with("--")) throw ConfigurationError("missing option value");
                if (option == "--config") config_path = std::string(args[i]);
                else if (args[i] == "free") format = MpsFormat::free;
                else if (args[i] == "fixed") format = MpsFormat::fixed;
                else if(option=="--mps-format") throw ConfigurationError("MPS format must be free or fixed");
                else if(option=="--method") { if(args[i]!="auto"&&args[i]!="simplex") throw ConfigurationError("method must be auto or simplex"); method=std::string(args[i]); }
                else if(option=="--branching") branching=std::string(args[i]);
                else if(option=="--time-limit"||option=="--mip-gap") {
                    char* end=nullptr; const std::string value(args[i]); const double parsed=std::strtod(value.c_str(),&end);
                    if(end!=value.c_str()+value.size()||!std::isfinite(parsed)||parsed<0) throw ConfigurationError("invalid numeric option");
                    if(option=="--time-limit") time_limit=parsed; else gap=parsed;
                } else {
                    Index parsed=0; const auto value=args[i]; const auto result=std::from_chars(value.data(),value.data()+value.size(),parsed);
                    if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()) throw ConfigurationError("invalid integer option");
                    if(option=="--node-limit") node_limit=parsed; else iteration_limit=parsed;
                }
            } else throw ConfigurationError("unknown option: " + std::string(option));
        }
        auto config = config_path ? read_config_file(*config_path) : Configuration{};
        if (format) config.mps.format = *format;
        if(branching) config.solver.branching=*branching;
        if(time_limit) config.solver.time_limit_seconds=*time_limit; if(gap) config.solver.mip_gap=*gap;
        if(node_limit) config.solver.node_limit=*node_limit; if(iteration_limit) config.solver.iteration_limit=*iteration_limit;
        validate_options(config.solver);
        Logger logger(errors,config.log_level);
        const auto model = read_mps_file(std::filesystem::path(path),config.mps);
        logger.write(LogLevel::info,"Model loaded and structurally validated: " + model.name);
        std::ostringstream result;
        result.imbue(std::locale::classic()); result << std::setprecision(17);
        if (args[0] == "inspect") print_statistics(result,model,json);
        else if(args[0]=="solve") {
            const bool integer=std::any_of(model.variables.begin(),model.variables.end(),[](const auto& v){return v.type!=VariableType::continuous;});
            const auto solved=integer ? solve_mip(model,config.solver) : solve_lp(model,config.solver);
            print_solution(result,model,solved,json);
            output<<result.str();
            if(!output) throw std::runtime_error("output write failure");
            return solved.status==SolveStatus::optimal||solved.status==SolveStatus::infeasible||solved.status==SolveStatus::unbounded ? 0 : (solved.status==SolveStatus::numerical_failure ? 4 : 5);
        }
        else if (json) result << "{\"name\":" << json_quote(model.name) << ",\"validation\":\"passed\",\"scope\":\"structural\"}\n";
        else result << "Structural validation: PASSED\nGeneral feasibility has not been determined.\n";
        output << result.str();
        if (!output) throw std::runtime_error("output write failure");
        return 0;
    } catch (const ConfigurationError& e) {
        Logger(errors).write(LogLevel::error,e.what()); return 2;
    } catch (const ModelParseError& e) {
        Logger(errors).write(LogLevel::error,e.what()); return 3;
    } catch (const InvalidModelError& e) {
        Logger(errors).write(LogLevel::error,e.what()); return 3;
    } catch (const std::exception& e) {
        Logger(errors).write(LogLevel::error,e.what()); return 4;
    }
}
}
