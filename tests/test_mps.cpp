#include <catch2/catch_test_macros.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/errors.hpp>
#include <sstream>
#include <fstream>
#include <chrono>
using namespace sovereign;
namespace {
Model parse(const std::string& s, MpsOptions options = {}) {
    std::istringstream in(s); return read_mps(in, options);
}
const std::string prefix = "NAME T\nROWS\n N OBJ\n L R\nCOLUMNS\n x OBJ 3 R 2\n";
std::string card(std::string f1, std::string f2, std::string f3, std::string f4, std::string f5 = {}, std::string f6 = {}) {
    std::string line(61,' ');
    for (auto [start, text] : {std::pair{1,f1}, {4,f2}, {14,f3}, {24,f4}, {39,f5}, {49,f6}}) line.replace(static_cast<std::size_t>(start),text.size(),text);
    return line + "\n";
}
}
TEST_CASE("MPS reads objective sense matrix defaults and objective offset", "[mps]") {
    const auto m = parse("NAME DEMO\nOBJSENSE\n MAX\nOBJNAME OBJ\nROWS\n N OBJ\n L R1\n G R2\n E R3\nCOLUMNS\n x OBJ 3 R1 1\n x R2 2 R3 1\n y OBJ 2 R1 1\nRHS\n rhs R1 4 R2 1\n rhs R3 2 OBJ -7\nENDATA\n");
    REQUIRE(m.name == "DEMO"); REQUIRE(m.sense == ObjectiveSense::maximize);
    REQUIRE(m.objective == std::vector<double>{3,2}); REQUIRE(m.objective_offset == 7);
    REQUIRE(m.matrix.nonzeros() == 4);
    REQUIRE(m.constraints[0].lower == -infinity); REQUIRE(m.constraints[0].upper == 4);
    REQUIRE(m.constraints[1].lower == 1); REQUIRE(m.constraints[1].upper == infinity);
    REQUIRE(m.constraints[2].lower == 2); REQUIRE(m.constraints[2].upper == 2);
    REQUIRE(m.variables[0].lower == 0); REQUIRE(m.variables[0].upper == infinity);
}
TEST_CASE("MPS ranges implement all row sign combinations", "[mps]") {
    for (const auto& type : {"L","G","E"}) for (const auto range : {"3","-3"}) {
        const auto m = parse(std::string("NAME T\nROWS\n N OBJ\n ")+type+" R\nCOLUMNS\n x R 1\nRHS\n b R 5\nRANGES\n q R "+range+"\nENDATA\n");
        const bool lower_shift = std::string(type)=="L" || (std::string(type)=="E" && std::string(range)=="-3");
        REQUIRE(m.constraints[0].lower == (lower_shift ? 2 : 5));
        REQUIRE(m.constraints[0].upper == (lower_shift ? 5 : 8));
    }
}
TEST_CASE("MPS bounds support continuous integer and binary domains", "[mps]") {
    struct Example { std::string records; double lo; double up; VariableType type; };
    for (const auto& e : std::vector<Example>{
        {" LO b x -2\n UP b x 4\n",-2,4,VariableType::continuous},
        {" FX b x 3\n",3,3,VariableType::continuous},
        {" FR b x\n",-infinity,infinity,VariableType::continuous},
        {" MI b x\n",-infinity,infinity,VariableType::continuous},
        {" PL b x\n",0,infinity,VariableType::continuous},
        {" BV b x\n",0,1,VariableType::binary},
        {" LI b x -2\n UI b x 4\n",-2,4,VariableType::integer},
        {" UP b x -2\n",-infinity,-2,VariableType::continuous}}) {
        const auto m = parse(prefix+"BOUNDS\n"+e.records+"ENDATA\n");
        REQUIRE(m.variables[0].lower == e.lo); REQUIRE(m.variables[0].upper == e.up); REQUIRE(m.variables[0].type == e.type);
    }
}
TEST_CASE("MPS integer markers use zero-one default and explicit general integer upper bounds", "[mps]") {
    const std::string s = "NAME T\nROWS\n N OBJ\n L R\nCOLUMNS\n M0 'MARKER' 'INTORG'\n x OBJ 1 R 1\n y R 1\n M1 'MARKER' 'INTEND'\n z R 1\nBOUNDS\n UP b x 9\nENDATA\n";
    const auto m = parse(s);
    REQUIRE(m.variables[0].type == VariableType::integer); REQUIRE(m.variables[0].upper == 9);
    REQUIRE(m.variables[1].type == VariableType::integer); REQUIRE(m.variables[1].upper == 1);
    REQUIRE(m.variables[2].type == VariableType::continuous); REQUIRE(m.variables[2].upper == infinity);
}
TEST_CASE("MPS handles fixed fields blank continuations comments and D exponents", "[mps]") {
    MpsOptions options; options.format = MpsFormat::fixed;
    const auto m = parse("* comment\nNAME FIXED\nROWS\n"+card("N","OBJ","","")+card("L","R","","")+"COLUMNS\n"+card("","x","OBJ","+2D0")+card("","","R","1")+"RHS\n"+card("","","R","3")+"BOUNDS\n"+card("UP","","x","5")+"ENDATA\n",options);
    REQUIRE(m.objective[0] == 2); REQUIRE(m.constraints[0].upper == 3); REQUIRE(m.variables[0].upper == 5);
}
TEST_CASE("MPS sums duplicate column entries and keeps structural zero variables", "[mps]") {
    const auto m = parse(prefix+" x R -2 OBJ 1\n y R 0\nENDATA\n");
    REQUIRE(m.matrix.nonzeros() == 0); REQUIRE(m.variables.size() == 2); REQUIRE(m.objective[0] == 4);
}
TEST_CASE("MPS rejects malformed unsupported and ambiguous records with source lines", "[mps]") {
    for (const auto& suffix : {
        "RHS\n b UNKNOWN 2\nENDATA\n", "RHS\n b R 2\n c R 3\nENDATA\n",
        "RHS\n b R 2\n b R 3\nENDATA\n", "RANGES\n b OBJ 2\nENDATA\n",
        "BOUNDS\n SC b x 2\nENDATA\n", "BOUNDS\n UP b missing 2\nENDATA\n",
        "BOUNDS\n UP b x 2\n FX b x 1\nENDATA\n", "BOUNDS\n UI b x 2.5\nENDATA\n",
        "BOUNDS\n LO b x 5\n UP b x 2\nENDATA\n", "QMATRIX\n x x 1\nENDATA\n",
        "RHS\n b R nan\nENDATA\n", "RHS\n b R 1e999\nENDATA\n", "RHS\n b R 2oops\nENDATA\n",
        "RHS\n b R\nENDATA\n", "ENDATA\n junk\n", "RHS\n b R 1\n"}) {
        INFO(suffix);
        try { (void)parse(prefix+suffix); FAIL("invalid MPS accepted"); }
        catch (const ModelParseError& error) { REQUIRE(error.line() > 0); }
    }
    REQUIRE_THROWS_AS(parse(""), ModelParseError);
    REQUIRE_THROWS_AS(parse("NAME T\nROWS\n N O\n N P\nCOLUMNS\nENDATA\n"), ModelParseError);
    REQUIRE_THROWS_AS(parse("NAME T\nROWS\n N O\n L R\n L R\nCOLUMNS\nENDATA\n"), ModelParseError);
    REQUIRE_THROWS_AS(parse(prefix+" M 'MARKER' 'INTORG'\nENDATA\n"), ModelParseError);
}
TEST_CASE("MPS resource limits fail explicitly", "[mps]") {
    MpsOptions options; options.max_line_length = 5;
    REQUIRE_THROWS_AS(parse(prefix+"ENDATA\n", options), ModelParseError);
    options = {}; options.max_entries = 1;
    REQUIRE_THROWS_AS(parse(prefix+"ENDATA\n", options), ModelParseError);
    REQUIRE_THROWS_AS(read_mps_file("does-not-exist.mps"), ModelParseError);
}
TEST_CASE("MPS rejects malformed double signs", "[mps][regression]") {
    REQUIRE_THROWS_AS(parse(prefix+"RHS\n b R +-1\nENDATA\n"), ModelParseError);
}
TEST_CASE("MPS rejects embedded control bytes", "[mps][regression]") {
    std::string nul_name = prefix + " y"; nul_name.push_back('\0'); nul_name += "z R 1\nENDATA\n";
    REQUIRE_THROWS_AS(parse(nul_name), ModelParseError);
}
TEST_CASE("MPS rejects non-ASCII bytes", "[mps][regression]") {
    REQUIRE_THROWS_AS(parse(prefix+" y\xFF R 1\nENDATA\n"), ModelParseError);
}
TEST_CASE("MPS detects stream failures overflow and invalid section transitions", "[mps]") {
    std::istringstream failed(prefix+"ENDATA\n"); failed.setstate(std::ios::badbit);
    REQUIRE_THROWS_AS(read_mps(failed), ModelParseError);
    for (const auto& text : {
        "NAME T\nCOLUMNS\nENDATA\n", "NAME T\nROWS\n N O\nCOLUMNS\n M 'MARKER' 'INTEND'\nENDATA\n",
        "NAME T\nOBJSENSE\nROWS\n N O\nCOLUMNS\nENDATA\n",
        "NAME T\nOBJNAME WRONG\nROWS\n N O\nCOLUMNS\nENDATA\n"}) REQUIRE_THROWS_AS(parse(text), ModelParseError);
    REQUIRE_THROWS_AS(parse(prefix+" x OBJ 1e308\n x OBJ 1e308\nENDATA\n"), ModelParseError);
    REQUIRE_THROWS_AS(parse(prefix+"RHS\n b R -1e308\nRANGES\n b R 1e308\nENDATA\n"), ModelParseError);
    REQUIRE_THROWS_AS(parse(prefix+"RANGES\n a R 2\n b R 1\nENDATA\n"), ModelParseError);
    REQUIRE_THROWS_AS(parse(prefix+"BOUNDS\n LO a x 0\n UP b x 2\nENDATA\n"), ModelParseError);
}
TEST_CASE("MPS preserves dollar-prefixed column and vector names", "[mps][regression]") {
    const auto m = parse("NAME DOLLAR\nROWS\n N OBJ\n L R\nCOLUMNS\n $X OBJ 1 R 2\nRHS\n $B R 6\nENDATA\n");
    REQUIRE(m.variables.size() == 1);
    REQUIRE(m.variables[0].name == "$X");
    REQUIRE(m.objective == std::vector<double>{1});
    REQUIRE(m.constraints[0].upper == 6);
    REQUIRE(m.matrix.column(0).values[0] == 2);
}
TEST_CASE("MPS accepts free inline comments only in row-name fields", "[mps]") {
    const auto m = parse(prefix+" x R 1 $ ignored second pair\nRHS\n b R 5 $ comment\nENDATA\n");
    REQUIRE(m.matrix.column(0).values[0] == 3);
    REQUIRE(m.constraints[0].upper == 5);
}
TEST_CASE("MPS file reader does not truncate at Windows Ctrl-Z", "[mps][regression]") {
    struct TemporaryFile {
        std::filesystem::path path;
        ~TemporaryFile() { std::error_code error; std::filesystem::remove(path,error); }
    } temp{std::filesystem::temp_directory_path() / ("sovereign-mps-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".mps")};
    std::ofstream file(temp.path,std::ios::binary);
    REQUIRE(file.is_open());
    file << prefix << "ENDATA\n" << '\x1a' << "trailing garbage\n";
    file.close(); REQUIRE(file.good());
    REQUIRE_THROWS_AS(read_mps_file(temp.path), ModelParseError);
}
