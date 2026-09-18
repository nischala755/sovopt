#include <catch2/catch_test_macros.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/recorder.hpp>
#include <sovereign/qp.hpp>
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
std::filesystem::path write_model(const char* name,std::string_view contents) {
    const auto path=std::filesystem::temp_directory_path()/name;
    std::ofstream(path,std::ios::binary|std::ios::trunc)<<contents;
    return path;
}
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

TEST_CASE("Flight Recorder persists and independently verifies every QP terminal proof", "[recorder][qp]") {
    SolverOptions options; options.method="interior_point";
    SECTION("optimal KKT certificate") {
        QuadraticModel qp{read_mps_file(model_path),CscMatrix::from_triplets(2,2,{{0,0,-0.2},{1,1,-0.2}})};
        const auto solved=solve_qp(qp,options); REQUIRE(solved.status==SolveStatus::optimal);
        const auto bundle=fresh_bundle("astraniti-qp-optimal.astra");record_qp_solve(bundle,model_path,qp,options,solved,{});
        const auto replay=replay_solve(bundle,true);INFO(replay.message);REQUIRE(replay.problem_kind=="quadratic");REQUIRE(replay.integrity_passed);REQUIRE(replay.reverification_passed);REQUIRE(std::filesystem::is_regular_file(bundle/"quadratic.json"));std::filesystem::remove_all(bundle);
    }
    SECTION("Farkas infeasibility certificate") {
        const auto source=write_model("astraniti-qp-infeasible.mps","NAME QPINF\nROWS\n N OBJ\n G LO\n L HI\nCOLUMNS\n x OBJ 0 LO 1\n x HI 1\nRHS\n rhs LO 1 HI 0\nBOUNDS\n FR bnd x\nENDATA\n");
        QuadraticModel qp{read_mps_file(source),CscMatrix::from_triplets(1,1,{{0,0,2}})};const auto solved=solve_qp(qp,options);REQUIRE(solved.status==SolveStatus::infeasible);
        const auto bundle=fresh_bundle("astraniti-qp-infeasible.astra");record_qp_solve(bundle,source,qp,options,solved,{});const auto replay=replay_solve(bundle,true);INFO(replay.message);REQUIRE(replay.reverification_passed);std::filesystem::remove_all(bundle);std::filesystem::remove(source);
    }
    SECTION("Hessian-null recession certificate") {
        const auto source=write_model("astraniti-qp-unbounded.mps","NAME QPUNB\nROWS\n N OBJ\nCOLUMNS\n x OBJ -1\nBOUNDS\n FR bnd x\nENDATA\n");
        QuadraticModel qp{read_mps_file(source),CscMatrix::from_triplets(1,1,{})};const auto solved=solve_qp(qp,options);REQUIRE(solved.status==SolveStatus::unbounded);
        const auto bundle=fresh_bundle("astraniti-qp-unbounded.astra");record_qp_solve(bundle,source,qp,options,solved,{});const auto replay=replay_solve(bundle,true);INFO(replay.message);REQUIRE(replay.reverification_passed);std::filesystem::remove_all(bundle);std::filesystem::remove(source);
    }
}

TEST_CASE("Flight Recorder detects a changed QP Hessian", "[recorder][qp][security]") {
    QuadraticModel qp{read_mps_file(model_path),CscMatrix::from_triplets(2,2,{{0,0,-0.2},{1,1,-0.2}})};SolverOptions options;options.method="interior_point";const auto solved=solve_qp(qp,options);REQUIRE(solved.status==SolveStatus::optimal);
    const auto bundle=fresh_bundle("astraniti-qp-tamper.astra");record_qp_solve(bundle,model_path,qp,options,solved,{});std::ofstream(bundle/"quadratic.json",std::ios::app)<<"tampered";const auto replay=replay_solve(bundle,true);REQUIRE_FALSE(replay.integrity_passed);REQUIRE(replay.tampered_artifact=="quadratic.json");std::filesystem::remove_all(bundle);
}

TEST_CASE("Flight Recorder replays linear terminal proofs with null objectives", "[recorder][regression]") {
    const auto source=write_model("astraniti-linear-infeasible.mps","NAME LPINF\nROWS\n N OBJ\n G LO\n L HI\nCOLUMNS\n x OBJ 0 LO 1\n x HI 1\nRHS\n rhs LO 1 HI 0\nBOUNDS\n FR bnd x\nENDATA\n");
    const auto model=read_mps_file(source);SolverOptions options;const auto solved=solve_lp(model,options);REQUIRE(solved.status==SolveStatus::infeasible);const auto bundle=fresh_bundle("astraniti-linear-infeasible.astra");record_solve(bundle,source,model,options,solved,{});const auto replay=replay_solve(bundle,true);INFO(replay.message);REQUIRE(replay.reverification_passed);std::filesystem::remove_all(bundle);std::filesystem::remove(source);
}
