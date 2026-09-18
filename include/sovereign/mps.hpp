#pragma once
#include <sovereign/model.hpp>
#include <sovereign/qp.hpp>
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
// QPS QUADOBJ entries define the triangular Q matrix in 0.5*x'Q*x.
[[nodiscard]] QuadraticModel read_qps(std::istream& input, const MpsOptions& options = {});
[[nodiscard]] QuadraticModel read_qps_file(const std::filesystem::path& path, const MpsOptions& options = {});
}
