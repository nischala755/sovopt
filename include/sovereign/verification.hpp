#pragma once
#include <sovereign/solution.hpp>
namespace sovereign {
[[nodiscard]] VerificationReport verify_primal(const Model&, std::span<const double>, double, const Tolerances& = {}, bool check_integrality = true);
[[nodiscard]] VerificationReport verify_optimality(const Model&, std::span<const double>, double, const DualCertificate&, const Tolerances& = {});
[[nodiscard]] VerificationReport verify_infeasibility(const Model&, const DualCertificate&, const Tolerances& = {});
[[nodiscard]] VerificationReport verify_unboundedness(const Model&, std::span<const double>, std::span<const double>, const Tolerances& = {}, bool check_integrality = false);
}
