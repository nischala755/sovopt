#include <catch2/catch_test_macros.hpp>
#include <sovereign/config.hpp>
#include <sovereign/errors.hpp>
#include <sstream>
#include <chrono>
#include <fstream>
using namespace sovereign;
TEST_CASE("Configuration defaults and flat YAML scalar overrides are explicit", "[config]") {
    std::istringstream empty;
    const auto defaults = read_config(empty);
    REQUIRE(defaults.log_level == LogLevel::info);
    REQUIRE(defaults.mps.format == MpsFormat::free);
    std::istringstream in("# local settings\nlog_level: debug\nmps_format: fixed # layout\nmax_line_length: 200\nmax_entries: 500\nfeasibility_pump: false\nfeasibility_pump_passes: 12\n");
    const auto config = read_config(in);
    REQUIRE(config.log_level == LogLevel::debug);
    REQUIRE(config.mps.format == MpsFormat::fixed);
    REQUIRE(config.mps.max_line_length == 200);
    REQUIRE(config.mps.max_entries == 500);
    REQUIRE_FALSE(config.solver.feasibility_pump);
    REQUIRE(config.solver.feasibility_pump_passes == 12);
}
TEST_CASE("Configuration rejects typos duplicate keys malformed numbers and unsupported YAML", "[config]") {
    for (const auto& s : {"unknown: 1", "max_entries: 0", "max_entries: -1", "max_entries: 2x", "max_entries: 999999999999999999999999999999", "log_level: bogus", "mps_format: auto", "log_level info", "log_level: info\nlog_level: debug", "nested:\n  log_level: debug", "log_level: 'info'", "max_entries: 1.5", "max_entries: +2"}) {
        std::istringstream in(s); INFO(s); REQUIRE_THROWS_AS(read_config(in), ConfigurationError);
    }
    REQUIRE_THROWS_AS(read_config_file("missing-config.yaml"), ConfigurationError);
}

TEST_CASE("Configuration rejects zero numerical tolerances", "[config]") {
    std::istringstream primal("primal_tolerance: 0\n");
    REQUIRE_THROWS_AS(read_config(primal), ConfigurationError);
    std::istringstream pivot("pivot_tolerance: 0.0\n");
    REQUIRE_THROWS_AS(read_config(pivot), ConfigurationError);
}
TEST_CASE("Configuration file reader does not truncate at Windows Ctrl-Z", "[config][regression]") {
    struct TemporaryFile {
        std::filesystem::path path;
        ~TemporaryFile() { std::error_code error; std::filesystem::remove(path,error); }
    } temp{std::filesystem::temp_directory_path() / ("sovereign-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".yaml")};
    std::ofstream file(temp.path,std::ios::binary);
    REQUIRE(file.is_open());
    file << "log_level: info\n" << '\x1a' << "unknown: 1\n";
    file.close(); REQUIRE(file.good());
    REQUIRE_THROWS_AS(read_config_file(temp.path), ConfigurationError);
}
