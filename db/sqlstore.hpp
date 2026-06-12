#pragma once
#include "../core/ast/json.hpp"
#include "pgwire.hpp"
#include <functional>
#include <string>
#include <vector>

namespace fve {

using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
using Exec = std::function<Rows(const std::string& sql, const std::vector<std::string>& params)>;

class SqlStore {
public:
    explicit SqlStore(Exec exec) : exec_(std::move(exec)) {}
    bool put(const std::string& coll, const std::string& id, const JsonP& doc);
    JsonP get(const std::string& coll, const std::string& id) const;
    bool exists(const std::string& coll, const std::string& id) const;
    std::vector<std::string> list(const std::string& coll) const;
    bool append(const std::string& log, const JsonP& entry);
    std::vector<JsonP> read_log(const std::string& log) const;
    static std::string ddl();

private:
    Exec exec_;
};

struct Transport {
    std::function<bool(const std::string&)> send;
    std::function<bool(std::string&)> recv;
};

class PgConn {
public:
    explicit PgConn(Transport t) : t_(std::move(t)) {}
    bool startup(const std::string& user, const std::string& db);
    Rows exec(const std::string& sql, const std::vector<std::string>& params);

private:
    Transport t_;
};

}
