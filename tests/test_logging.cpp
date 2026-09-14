#include <catch2/catch_test_macros.hpp>
#include <sovereign/logging.hpp>
#include <sovereign/errors.hpp>
#include <sstream>
#include <thread>
#include <vector>
using namespace sovereign;
TEST_CASE("Logger filters severity and escapes JSON control characters", "[logging]") {
    std::ostringstream out;
    Logger logger(out,LogLevel::warn);
    logger.write(LogLevel::trace,"hidden"); logger.write(LogLevel::debug,"hidden"); logger.write(LogLevel::info,"hidden");
    logger.write(LogLevel::warn,"quote\" slash\\ newline\n tab\t\r\b\f\x01");
    logger.write(LogLevel::error,"failure");
    REQUIRE(out.str() == "{\"level\":\"warn\",\"message\":\"quote\\\" slash\\\\ newline\\n tab\\t\\r\\b\\f\\u0001\"}\n{\"level\":\"error\",\"message\":\"failure\"}\n");
}
TEST_CASE("Logger serializes concurrent records without interleaving", "[logging]") {
    std::ostringstream out; Logger logger(out,LogLevel::trace);
    std::vector<std::jthread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([&]{ for (int j = 0; j < 50; ++j) logger.write(LogLevel::debug,"event"); });
    workers.clear();
    std::istringstream lines(out.str()); std::string line; int count = 0;
    while (std::getline(lines,line)) { REQUIRE(line == "{\"level\":\"debug\",\"message\":\"event\"}"); ++count; }
    REQUIRE(count == 200);
}
TEST_CASE("Logger rejects invalid levels and preserves ordinary text", "[logging]") {
    std::ostringstream out;
    REQUIRE_THROWS_AS(Logger(out,static_cast<LogLevel>(99)), ConfigurationError);
    Logger logger(out);
    REQUIRE_THROWS_AS(logger.write(static_cast<LogLevel>(-1),"bad"), ConfigurationError);
    REQUIRE(json_quote("plain") == "\"plain\"");
    REQUIRE(json_quote(std::string("a\0b",3)) == "\"a\\u0000b\"");
}
