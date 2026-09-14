#include <sovereign/config.hpp>
#include <sovereign/errors.hpp>
#include <charconv>
#include <fstream>
#include <unordered_set>
#include <cstdlib>
#include <cmath>

namespace sovereign {
namespace {
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
}
Configuration read_config(std::istream& input) {
    Configuration result;
    std::unordered_set<std::string> keys;
    Index line_number = 0;
    const auto fail = [&](const std::string& message) { throw ConfigurationError("config line " + std::to_string(line_number) + ": " + message); };
    for (std::string line; std::getline(input,line);) {
        ++line_number;
        line = line.substr(0,line.find('#'));
        if (trim(line).empty()) continue;
        if (line[0] == ' ' || line[0] == '\t') fail("nested or indented YAML is unsupported");
        const auto colon = line.find(':');
        if (colon == std::string::npos) fail("expected key: value");
        const auto key = trim(line.substr(0,colon)); const auto value = trim(line.substr(colon+1));
        if (value.empty() || !keys.insert(key).second) fail("empty value or duplicate key: " + key);
        if (key == "log_level") {
            if (value == "trace") result.log_level = LogLevel::trace;
            else if (value == "debug") result.log_level = LogLevel::debug;
            else if (value == "info") result.log_level = LogLevel::info;
            else if (value == "warn") result.log_level = LogLevel::warn;
            else if (value == "error") result.log_level = LogLevel::error;
            else fail("unknown log level");
        } else if (key == "mps_format") {
            if (value == "free") result.mps.format = MpsFormat::free;
            else if (value == "fixed") result.mps.format = MpsFormat::fixed;
            else fail("mps_format must be free or fixed");
        } else if (key == "branching") {
            if(value!="most_fractional"&&value!="pseudocost") fail("unsupported branching strategy");
            result.solver.branching=value;
        } else if (key == "scaling" || key == "presolve" || key == "cuts" || key == "rounding" || key == "deterministic") {
            if(value!="true"&&value!="false") fail("expected true or false");
            const bool enabled=value=="true";
            if(key=="scaling") result.solver.scaling=enabled; else if(key=="presolve") result.solver.presolve=enabled;
            else if(key=="cuts") result.solver.cuts=enabled; else if(key=="rounding") result.solver.rounding=enabled; else result.solver.deterministic=enabled;
        } else if (key == "time_limit" || key == "mip_gap" || key == "primal_tolerance" || key == "dual_tolerance" || key == "integrality_tolerance" || key == "pivot_tolerance") {
            char* end=nullptr; const double number=std::strtod(value.c_str(),&end);
            if(end!=value.c_str()+value.size()||!std::isfinite(number)||number<0) fail("expected finite nonnegative number");
            if (number == 0 && key != "time_limit" && key != "mip_gap") fail("solver tolerances must be positive");
            if(key=="time_limit") result.solver.time_limit_seconds=number; else if(key=="mip_gap") result.solver.mip_gap=number;
            else if(key=="primal_tolerance") result.solver.tolerances.primal=number; else if(key=="dual_tolerance") result.solver.tolerances.dual=number;
            else if(key=="integrality_tolerance") result.solver.tolerances.integrality=number; else result.solver.tolerances.pivot=number;
        } else if (key == "iteration_limit" || key == "node_limit" || key == "max_line_length" || key == "max_entries") {
            Index size = 0;
            const auto parsed = std::from_chars(value.data(),value.data()+value.size(),size);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size() || size == 0) fail("expected positive integer");
            if (key == "max_line_length") result.mps.max_line_length = size;
            else if(key=="max_entries") result.mps.max_entries = size;
            else if(key=="iteration_limit") result.solver.iteration_limit=size;
            else result.solver.node_limit=size;
        } else fail("unknown key: " + key);
    }
    if (input.bad() || (input.fail() && !input.eof())) fail("configuration read failure");
    return result;
}
Configuration read_config_file(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw ConfigurationError("cannot open configuration: " + path.string());
    return read_config(input);
}
}
