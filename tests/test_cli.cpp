#include <catch2/catch_test_macros.hpp>
#include <sovereign/cli.hpp>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
using namespace sovereign;
namespace {
struct Result { int code; std::string out; std::string err; };
Result run(std::vector<std::string_view> args) {
    std::ostringstream out,err; const int code = run_cli(args,out,err); return {code,out.str(),err.str()};
}
const std::string lp = std::string(SOVEREIGN_SOURCE_DIR) + "/examples/small_lp.mps";
}
TEST_CASE("CLI help version and unsupported commands report honest capabilities", "[cli]") {
    REQUIRE(run({"--help"}).code == 0);
    REQUIRE(run({"--help"}).out.find("inspect") != std::string::npos);
    REQUIRE(run({"--version"}).code == 0);
    for (auto args : std::vector<std::vector<std::string_view>>{{},{"inspect"},{"inspect",lp,"--unknown"},{"inspect",lp,"--config"},{"inspect",lp,"--json","--json"},{"inspect",lp,"--mps-format","auto"},{"--help","junk"}}) {
        const auto r = run(args); REQUIRE(r.code == 2); REQUIRE(r.out.empty()); REQUIRE_FALSE(r.err.empty());
    }
}
TEST_CASE("CLI solve returns a verified mathematical result", "[cli]") {
    const auto r=run({"solve",lp,"--json","--method","simplex"});
    REQUIRE(r.code==0); REQUIRE(r.out.find("\"status\":\"optimal\"")!=std::string::npos);
    REQUIRE(r.out.find("\"objective\":9")!=std::string::npos); REQUIRE(r.out.find("\"verified\":true")!=std::string::npos);
}
TEST_CASE("CLI exposes the verified interior point LP method", "[cli][qp]") {
    const auto r=run({"solve",lp,"--json","--method","interior_point"}); INFO(r.err);
    REQUIRE(r.code==0); REQUIRE(r.out.find("\"status\":\"optimal\"")!=std::string::npos);
    REQUIRE(r.out.find("\"verified\":true")!=std::string::npos);
}
TEST_CASE("CLI inspects example with hand-counted statistics and validates without solving", "[cli]") {
    const auto r = run({"inspect",lp,"--json"});
    REQUIRE(r.code == 0);
    REQUIRE(r.out.find("\"variables\":2") != std::string::npos);
    REQUIRE(r.out.find("\"constraints\":2") != std::string::npos);
    REQUIRE(r.out.find("\"nonzeros\":4") != std::string::npos);
    REQUIRE(r.out.find("\"objective_sense\":\"maximize\"") != std::string::npos);
    const auto text = run({"inspect",lp}); REQUIRE(text.code == 0); REQUIRE(text.out.find("Variables: 2") != std::string::npos);
    const auto v = run({"validate",lp,"--json"}); REQUIRE(v.code == 0); REQUIRE(v.out.find("\"validation\":\"passed\"") != std::string::npos);
    REQUIRE(v.out.find("\"scope\":\"structural\"") != std::string::npos);
}
TEST_CASE("CLI returns distinct errors for model and configuration failures", "[cli]") {
    REQUIRE(run({"inspect","missing.mps"}).code == 3);
    const auto invalid = run({"inspect",lp,"--config","missing.yaml","--json"});
    REQUIRE(invalid.code == 2); REQUIRE(invalid.out.empty()); REQUIRE(invalid.err.front() == '{');
    const std::string config = std::string(SOVEREIGN_SOURCE_DIR)+"/examples/inspect.yaml";
    REQUIRE(run({"inspect",lp,"--config",config}).code == 0);
}
TEST_CASE("CLI detects output failure and explicit format overrides configuration", "[cli]") {
    std::ostringstream out,err; out.setstate(std::ios::badbit);
    const std::vector<std::string_view> args{"inspect",lp};
    REQUIRE(run_cli(args,out,err) == 4);
    const std::string config = std::string(SOVEREIGN_SOURCE_DIR)+"/examples/inspect.yaml";
    REQUIRE(run({"inspect",lp,"--config",config,"--mps-format","fixed"}).code == 3);
    REQUIRE(run({"inspect",lp,"--config",config,"--mps-format","free"}).code == 0);
}

TEST_CASE("CLI records and replays tamper evident solves", "[cli][recorder]") {
    const auto bundle=std::filesystem::temp_directory_path()/"astraniti-cli-record.astra";
    std::filesystem::remove_all(bundle); const auto bundle_text=bundle.string();
    auto recorded=run({"solve",lp,"--record",bundle_text});
    INFO(recorded.err); REQUIRE(recorded.code==0); REQUIRE(std::filesystem::is_directory(bundle));
    auto replayed=run({"replay",bundle_text,"--reverify"});
    INFO(replayed.err); REQUIRE(replayed.code==0); REQUIRE(replayed.out.find("Integrity: PASS")!=std::string::npos);
    REQUIRE(replayed.out.find("Reverification: PASS")!=std::string::npos);
    std::ofstream(bundle/"solution.json",std::ios::app)<<"tamper";
    auto tampered=run({"replay",bundle_text});
    REQUIRE(tampered.code!=0); REQUIRE(tampered.out.find("TAMPER DETECTED")!=std::string::npos);
    std::filesystem::remove_all(bundle);
}
