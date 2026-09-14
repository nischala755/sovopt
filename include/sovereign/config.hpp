#pragma once
#include <sovereign/logging.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/solution.hpp>

namespace sovereign {
struct Configuration { LogLevel log_level = LogLevel::info; MpsOptions mps; SolverOptions solver; };
// Strict flat YAML subset: unique keys, unquoted scalar values, optional # comments.
[[nodiscard]] Configuration read_config(std::istream& input);
[[nodiscard]] Configuration read_config_file(const std::filesystem::path& path);
}
