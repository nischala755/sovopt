#pragma once
#include <sovereign/model.hpp>
#include <cstdint>

namespace sovereign {
[[nodiscard]] Model generate_refinery_model(std::uint64_t seed, Index periods = 8);
[[nodiscard]] Model generate_power_model(std::uint64_t seed, Index units = 12);
[[nodiscard]] Model generate_logistics_model(std::uint64_t seed, Index locations = 10);
}
