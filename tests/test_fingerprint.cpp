#include <catch2/catch_test_macros.hpp>
#include <sovereign/fingerprint.hpp>
#include <sovereign/generators.hpp>
#include <sovereign/validation.hpp>
#include <cmath>

using namespace sovereign;

TEST_CASE("Fingerprint is deterministic and exact model fields affect its hash", "[fingerprint]") {
    auto a = generate_refinery_model(17, 3);
    auto b = generate_refinery_model(17, 3);
    const auto fa = fingerprint(a);
    const auto fb = fingerprint(b);
    REQUIRE(fa.stable_hash == fb.stable_hash);
    REQUIRE(fa.hash_hex == fb.hash_hex);
    REQUIRE(fa.nonzeros == a.matrix.nonzeros());
    REQUIRE(fa.estimated_bytes > 0);
    b.objective[0] = std::nextafter(b.objective[0], infinity);
    REQUIRE(fingerprint(b).stable_hash != fa.stable_hash);
    b = a;
    b.variables[0].name += "x";
    REQUIRE(fingerprint(b).stable_hash != fa.stable_hash);
}

TEST_CASE("Seeded industrial generators are reproducible and valid", "[fingerprint][generators]") {
    const auto refinery = generate_refinery_model(11, 2);
    const auto power = generate_power_model(11, 3);
    const auto logistics = generate_logistics_model(11, 3);
    REQUIRE(validate(refinery).ok());
    REQUIRE(validate(power).ok());
    REQUIRE(validate(logistics).ok());
    REQUIRE(fingerprint(refinery).stable_hash == fingerprint(generate_refinery_model(11, 2)).stable_hash);
    REQUIRE(fingerprint(power).stable_hash == fingerprint(generate_power_model(11, 3)).stable_hash);
    REQUIRE(fingerprint(logistics).stable_hash == fingerprint(generate_logistics_model(11, 3)).stable_hash);
    REQUIRE(fingerprint(refinery).stable_hash != fingerprint(generate_refinery_model(12, 2)).stable_hash);
}
