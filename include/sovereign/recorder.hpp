#pragma once
#include <sovereign/fingerprint.hpp>
#include <sovereign/qp.hpp>
#include <sovereign/solution.hpp>
#include <filesystem>

namespace sovereign {
inline constexpr Index flight_recorder_format_version = 2;

struct ReplayReport {
    bool integrity_passed = false;
    bool reverification_passed = false;
    std::string recorded_status;
    std::string problem_kind = "linear";
    std::string message;
    std::string tampered_artifact;
    std::string expected_sha256;
    std::string actual_sha256;
    std::vector<TelemetryEvent> timeline;
};

void record_solve(const std::filesystem::path& bundle,
                  const std::filesystem::path& model_source,
                  const Model& model,
                  const SolverOptions& options,
                  const SolveResult& result,
                  std::span<const TelemetryEvent> events);

void record_qp_solve(const std::filesystem::path& bundle,
                     const std::filesystem::path& model_source,
                     const QuadraticModel& model,
                     const SolverOptions& options,
                     const QpResult& result,
                     std::span<const TelemetryEvent> events);

[[nodiscard]] ReplayReport replay_solve(const std::filesystem::path& bundle,
                                        bool reverify = false);
}
