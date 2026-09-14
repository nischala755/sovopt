#pragma once
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>

namespace sovereign {
enum class LogLevel { trace, debug, info, warn, error };
[[nodiscard]] std::string json_quote(std::string_view text);
class Logger {
public:
    explicit Logger(std::ostream& output, LogLevel minimum = LogLevel::info);
    void write(LogLevel level, std::string_view message);
private:
    std::ostream& output_;
    LogLevel minimum_;
    std::mutex mutex_;
};
}
