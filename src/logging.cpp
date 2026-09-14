#include <sovereign/logging.hpp>
#include <sovereign/errors.hpp>
#include <array>

namespace sovereign {
namespace {
constexpr std::array<std::string_view,5> levels{"trace","debug","info","warn","error"};
bool valid_level(LogLevel level) { return level >= LogLevel::trace && level <= LogLevel::error; }
}
std::string json_quote(std::string_view text) {
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        default:
            if (c < 0x20) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
            else result += static_cast<char>(c);
        }
    }
    result += '"';
    return result;
}
Logger::Logger(std::ostream& output, LogLevel minimum) : output_(output), minimum_(minimum) {
    if (!valid_level(minimum)) throw ConfigurationError("invalid log level");
}
void Logger::write(LogLevel level, std::string_view message) {
    if (!valid_level(level)) throw ConfigurationError("invalid log level");
    if (level < minimum_) return;
    const auto record = "{\"level\":" + json_quote(levels[static_cast<std::size_t>(level)]) + ",\"message\":" + json_quote(message) + "}\n";
    const std::lock_guard lock(mutex_);
    output_ << record;
}
}
