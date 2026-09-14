#include <catch2/catch_test_macros.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/recorder.hpp>
#include <filesystem>
#include <algorithm>
#include <fstream>
using namespace sovereign;

namespace {
std::filesystem::path fresh_bundle(const char* name) {
    auto path=std::filesystem::temp_directory_path()/name;
    std::filesystem::remove_all(path);
    return path;
}
const auto model_path=std::filesystem::path(SOVEREIGN_SOURCE_DIR)/"examples"/"small_lp.mps";
}

TEST_CASE("Flight Recorder creates a complete bundle and independently reverifies it", "[recorder]") {
    const auto model=read_mps_file(model_path);
    SolverOptions options; std::vector<TelemetryEvent> events;
    options.telemetry=[&](const auto& event){events.push_back(event);};
    const auto solved=solve_lp(model,options);
    const auto bundle=fresh_bundle("astraniti-recorder-pass.astra");
    record_solve(bundle,model_path,model,options,solved,events);
    for (const auto* artifact:{"manifest.json","model.mps","model_fingerprint.json","solver_config.json",
        "presolve.json","telemetry.json","timeline.json","solution.json","certificate.json","verification.json","checksums.json"})
        REQUIRE(std::filesystem::is_regular_file(bundle/artifact));
    const auto replay=replay_solve(bundle,true);
    INFO(replay.message);
    REQUIRE(replay.integrity_passed);
    REQUIRE(replay.reverification_passed);
    REQUIRE(replay.recorded_status=="optimal");
    REQUIRE_FALSE(replay.timeline.empty());
    REQUIRE(std::any_of(replay.timeline.begin(),replay.timeline.end(),[](const auto& event){
        return event.type=="FACTORIZATION" && event.detail.find("basis dimension=")!=std::string::npos;
    }));
    std::filesystem::remove_all(bundle);
}

TEST_CASE("Flight Recorder detects changed mathematical artifacts", "[recorder][security]") {
    const auto model=read_mps_file(model_path); SolverOptions options;
    const auto solved=solve_lp(model,options);
    for (const auto* artifact:{"solution.json","model_fingerprint.json","certificate.json"}) {
        const auto bundle=fresh_bundle((std::string("astraniti-tamper-")+artifact+".astra").c_str());
        record_solve(bundle,model_path,model,options,solved,{});
        std::ofstream(bundle/artifact,std::ios::app)<<"tampered";
        const auto replay=replay_solve(bundle,false);
        REQUIRE_FALSE(replay.integrity_passed);
        REQUIRE(replay.tampered_artifact==artifact);
        REQUIRE_FALSE(replay.expected_sha256.empty());
        REQUIRE_FALSE(replay.actual_sha256.empty());
        std::filesystem::remove_all(bundle);
    }
}

TEST_CASE("Flight Recorder rejects missing and unsupported bundles", "[recorder][security]") {
    const auto missing=fresh_bundle("astraniti-recorder-missing.astra");
    auto replay=replay_solve(missing,false);
    REQUIRE_FALSE(replay.integrity_passed);
    std::filesystem::create_directories(missing);
    std::ofstream(missing/"manifest.json")<<"{\"format_version\":99}";
    replay=replay_solve(missing,false);
    REQUIRE_FALSE(replay.integrity_passed);
    std::filesystem::remove_all(missing);
}
