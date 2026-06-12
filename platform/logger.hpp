#pragma once
#include "../core/ast/json.hpp"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace fve {

enum class LogLevel { Debug, Info, Warn, Error, Alert };

using LogField = std::pair<std::string, std::string>;

class Logger {
public:
    static Logger from_env(); // reads FVE_LOG_FORMAT, FVE_LOG_LEVEL

    Logger(bool json_mode, LogLevel level) : level_(level), json_mode_(json_mode) {}

    void stage(const std::string& stage, const std::string& msg,
               const std::vector<LogField>& fields = {}, LogLevel lvl = LogLevel::Info);

    void alert(const std::string& alert_type, const std::string& subject,
               const std::vector<LogField>& fields = {});

    // Throttled: emits at most once per `min_interval_s` seconds regardless of call rate.
    void progress(const std::string& stage, const std::vector<LogField>& fields,
                  double min_interval_s = 30.0);

    bool json() const { return json_mode_; }

private:
    void emit(const std::string& stage, const std::string& event, const std::string& msg,
              const std::vector<LogField>& fields, LogLevel lvl);
    LogLevel level_;
    bool json_mode_;
    std::mutex mu_;
    std::chrono::steady_clock::time_point last_progress_{};
    bool have_progress_ = false;
};

}
