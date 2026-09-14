#pragma once
#include <sovereign/model.hpp>
#include <filesystem>
#include <istream>

namespace sovereign {
enum class MpsFormat { free, fixed };
struct MpsOptions {
    MpsFormat format = MpsFormat::free;
    Index max_line_length = 1024 * 1024;
    Index max_entries = 10000000; // Input coefficient pairs, before aggregation.
};
[[nodiscard]] Model read_mps(std::istream& input, const MpsOptions& options = {});
[[nodiscard]] Model read_mps_file(const std::filesystem::path& path, const MpsOptions& options = {});
}
