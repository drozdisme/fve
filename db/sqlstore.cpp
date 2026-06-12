#include "sqlstore.hpp"

namespace fve {

std::string SqlStore::ddl() {
    return "CREATE TABLE IF NOT EXISTS documents (coll TEXT, id TEXT, body JSONB, "
           "PRIMARY KEY (coll, id));\n"
           "CREATE TABLE IF NOT EXISTS logs (log TEXT, seq BIGINT, entry JSONB, "
           "PRIMARY KEY (log, seq));\n";
}

bool SqlStore::put(const std::string& coll, const std::string& id, const JsonP& doc) {
    exec_("INSERT INTO documents(coll,id,body) VALUES($1,$2,$3) "
          "ON CONFLICT (coll,id) DO UPDATE SET body=EXCLUDED.body",
          {coll, id, dump(doc)});
    return true;
}

JsonP SqlStore::get(const std::string& coll, const std::string& id) const {
    Rows r = exec_("SELECT body FROM documents WHERE coll=$1 AND id=$2", {coll, id});
    if (r.empty() || r[0].empty()) return nullptr;
    return parse(r[0][0]);
}

bool SqlStore::exists(const std::string& coll, const std::string& id) const {
    Rows r = exec_("SELECT 1 FROM documents WHERE coll=$1 AND id=$2", {coll, id});
    return !r.empty();
}

std::vector<std::string> SqlStore::list(const std::string& coll) const {
    Rows r = exec_("SELECT id FROM documents WHERE coll=$1", {coll});
    std::vector<std::string> out;
    for (const auto& row : r)
        if (!row.empty()) out.push_back(row[0]);
    return out;
}

bool SqlStore::append(const std::string& log, const JsonP& entry) {
    exec_("INSERT INTO logs(log,seq,entry) VALUES($1,"
          "COALESCE((SELECT MAX(seq) FROM logs WHERE log=$1),-1)+1,$2)",
          {log, dump(entry)});
    return true;
}

std::vector<JsonP> SqlStore::read_log(const std::string& log) const {
    Rows r = exec_("SELECT entry FROM logs WHERE log=$1 ORDER BY seq", {log});
    std::vector<JsonP> out;
    for (const auto& row : r)
        if (!row.empty()) out.push_back(parse(row[0]));
    return out;
}

bool PgConn::startup(const std::string& user, const std::string& db) {
    if (!t_.send(pg_startup(user, db))) return false;
    std::string buf;
    std::string chunk;
    while (t_.recv(chunk)) {
        buf += chunk;
        chunk.clear();
        size_t off = 0;
        PgMsg m;
        while (pg_read_msg(buf, off, m)) {
            if (m.type == 'Z') return true;
            if (m.type == 'E') return false;
        }
    }
    return false;
}

Rows PgConn::exec(const std::string& sql, const std::vector<std::string>& params) {
    std::string out;
    out += pg_parse("", sql);
    out += pg_bind("", "", params);
    out += pg_describe_portal("");
    out += pg_execute("", 0);
    out += pg_sync();
    Rows rows;
    if (!t_.send(out)) return rows;
    std::string buf;
    std::string chunk;
    while (t_.recv(chunk)) {
        buf += chunk;
        chunk.clear();
        size_t off = 0;
        PgMsg m;
        size_t last = 0;
        while (pg_read_msg(buf, off, m)) {
            last = off;
            if (m.type == 'D') rows.push_back(pg_data_row(m.payload));
            else if (m.type == 'Z') return rows;
        }
        buf.erase(0, last);
    }
    return rows;
}

}
