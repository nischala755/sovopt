#pragma once
#include <sovereign/model.hpp>
#include <cstdint>

namespace sovereign {
[[nodiscard]] Model generate_refinery_model(std::uint64_t seed, Index periods = 8);
[[nodiscard]] Model generate_power_model(std::uint64_t seed, Index units = 12);
[[nodiscard]] Model generate_logistics_model(std::uint64_t seed, Index locations = 10);
[[nodiscard]] Model generate_crude_blending_model(std::uint64_t seed, Index crudes = 6);
[[nodiscard]] Model generate_production_planning_model(std::uint64_t seed, Index products = 6, Index periods = 8);
[[nodiscard]] Model generate_supply_chain_model(std::uint64_t seed, Index plants = 4, Index customers = 8);
[[nodiscard]] Model generate_process_model(std::uint64_t seed, Index stages = 8);
}
