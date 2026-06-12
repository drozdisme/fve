#include "logger.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace fve {

namespace {

std::string iso_now() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    gmtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

std::string clock_hms() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&t, &tmv);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
    return buf;
}

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

}

Logger Logger::from_env() {
    const char* fmt = std::getenv("FVE_LOG_FORMAT");
    const char* lvl = std::getenv("FVE_LOG_LEVEL");
    bool json = fmt && std::string(fmt) == "json";
    LogLevel level = LogLevel::Info;
    if (lvl) {
        std::string l = lvl;
        if (l == "debug") level = LogLevel::Debug;
        else if (l == "warn") level = LogLevel::Warn;
        else if (l == "error") level = LogLevel::Error;
    }
    return Logger(json, level);
}

void Logger::emit(const std::string& stage, const std::string& event, const std::string& msg,
                  const std::vector<LogField>& fields, LogLevel lvl) {
    if (lvl < level_ && lvl != LogLevel::Alert) return;
    std::lock_guard<std::mutex> lk(mu_);
    if (json_mode_) {
        std::string line = "{\"ts\":\"" + iso_now() + "\",\"stage\":\"" + stage +
                           "\",\"event\":\"" + event + "\"";
        if (!msg.empty()) line += ",\"msg\":\"" + json_escape(msg) + "\"";
        for (const auto& f : fields) line += ",\"" + f.first + "\":\"" + json_escape(f.second) + "\"";
        line += "}";
        std::fprintf(stdout, "%s\n", line.c_str());
    } else {
        std::string line = "[" + clock_hms() + "] ";
        std::string st = upper(stage);
        st.resize(8, ' ');
        line += st + " ";
        std::string m = msg;
        m.resize(m.size() < 32 ? 32 : m.size(), ' ');
        line += m;
        for (const auto& f : fields) line += " " + f.first + "=" + f.second;
        std::fprintf(stdout, "%s\n", line.c_str());
    }
    std::fflush(stdout);
}

void Logger::stage(const std::string& stage, const std::string& msg,
                   const std::vector<LogField>& fields, LogLevel lvl) {
    emit(stage, "info", msg, fields, lvl);
}

void Logger::alert(const std::string& alert_type, const std::string& subject,
                   const std::vector<LogField>& fields) {
    std::vector<LogField> f = fields;
    f.insert(f.begin(), {"type", alert_type});
    f.insert(f.begin() + 1, {"subject", subject});
    emit("alert", "alert", alert_type, f, LogLevel::Alert);
}

void Logger::progress(const std::string& stage, const std::vector<LogField>& fields,
                      double min_interval_s) {
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (have_progress_) {
            double dt = std::chrono::duration<double>(now - last_progress_).count();
            if (dt < min_interval_s) return;
        }
        last_progress_ = now;
        have_progress_ = true;
    }
    emit(stage, "progress", "progress", fields, LogLevel::Info);
}

}
