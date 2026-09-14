#pragma once
#include <sovereign/model.hpp>

namespace sovereign {
struct ValidationIssue { std::string code; std::string message; };
struct ValidationReport {
    std::vector<ValidationIssue> issues;
    [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};
[[nodiscard]] ValidationReport validate(const Model& model);
void require_valid(const Model& model);
}
