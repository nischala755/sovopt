#pragma once
#include <ostream>
#include <span>
#include <string_view>

namespace sovereign {
// Arguments exclude argv[0]. Returns 0 success, 2 usage/config, 3 model, 4 runtime/I/O.
int run_cli(std::span<const std::string_view> arguments, std::ostream& output, std::ostream& errors);
}
