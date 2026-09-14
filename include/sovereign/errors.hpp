#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>

namespace sovereign {
class InvalidModelError : public std::runtime_error { using std::runtime_error::runtime_error; };
class ConfigurationError : public std::runtime_error { using std::runtime_error::runtime_error; };
class ModelParseError : public std::runtime_error {
public:
    ModelParseError(std::size_t line, const std::string& message)
        : std::runtime_error("MPS line " + std::to_string(line) + ": " + message), line_(line) {}
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
private:
    std::size_t line_;
};
}
