#include "../db/sqlstore.hpp"
#include "../db/pgwire.hpp"
#include "framework.hpp"
#include <map>

using namespace fve;
using namespace fvetest;

namespace {

void put_i32(std::string& s, uint32_t v) {
    s += (char)(v >> 24);
    s += (char)(v >> 16);
    s += (char)(v >> 8);
    s += (char)v;
}
void put_i16(std::string& s, uint16_t v) {
    s += (char)(v >> 8);
    s += (char)v;
}
std::string framed(char t, const std::string& p) {
    std::string s;
    s += t;
    put_i32(s, (uint32_t)(p.size() + 4));
    s += p;
    return s;
}

struct MemDb {
    std::map<std::pair<std::string, std::string>, std::string> docs;
    std::map<std::string, std::vector<std::string>> logs;
    std::vector<std::string> seen_sql;

    Rows exec(const std::string& sql, const std::vector<std::string>& p) {
        seen_sql.push_back(sql);
        if (sql.find("INSERT INTO documents") == 0) {
            docs[{p[0], p[1]}] = p[2];
            return {};
        }
        if (sql.find("SELECT body FROM documents") == 0) {
            auto it = docs.find({p[0], p[1]});
            if (it == docs.end()) return {};
            return {{it->second}};
        }
        if (sql.find("SELECT 1 FROM documents") == 0) {
            return docs.count({p[0], p[1]}) ? Rows{{"1"}} : Rows{};
        }
        if (sql.find("SELECT id FROM documents") == 0) {
            Rows r;
            for (auto& kv : docs)
                if (kv.first.first == p[0]) r.push_back({kv.first.second});
            return r;
        }
        if (sql.find("INSERT INTO logs") == 0) {
            logs[p[0]].push_back(p[1]);
            return {};
        }
        if (sql.find("SELECT entry FROM logs") == 0) {
            Rows r;
            for (auto& e : logs[p[0]]) r.push_back({e});
            return r;
        }
        return {};
    }
};

}

void test_sqldb() {
    cur = "sqldb";

    std::string su = pg_startup("fve", "fve");
    uint32_t slen = ((uint8_t)su[0] << 24) | ((uint8_t)su[1] << 16) | ((uint8_t)su[2] << 8) | (uint8_t)su[3];
    CHECK(slen == su.size());
    CHECK(su.find(std::string("user") + '\0' + "fve") != std::string::npos);
    CHECK(su.find(std::string("database") + '\0' + "fve") != std::string::npos);

    std::string q = pg_query("SELECT 1");
    CHECK(q[0] == 'Q');
    CHECK(q.find("SELECT 1") != std::string::npos);
    CHECK(pg_parse("", "X")[0] == 'P');
    CHECK(pg_bind("", "", {"a"})[0] == 'B');
    CHECK(pg_execute("", 0)[0] == 'E');
    CHECK(pg_sync()[0] == 'S');
    CHECK(pg_terminate()[0] == 'X');

    std::string rd;
    put_i16(rd, 1);
    rd += "body";
    rd += '\0';
    rd += std::string(18, '\0');
    auto names = pg_row_description(rd);
    CHECK(names.size() == 1 && names[0] == "body");

    std::string dr;
    put_i16(dr, 2);
    put_i32(dr, 5);
    dr += "hello";
    put_i32(dr, 3);
    dr += "abc";
    auto cols = pg_data_row(dr);
    CHECK(cols.size() == 2 && cols[0] == "hello" && cols[1] == "abc");

    std::string stream = framed('T', rd) + framed('D', dr) + framed('C', std::string("SELECT 1") + '\0') + framed('Z', "I");
    size_t off = 0;
    PgMsg m;
    std::vector<char> types;
    while (pg_read_msg(stream, off, m)) types.push_back(m.type);
    CHECK(types.size() == 4);
    CHECK(types[0] == 'T' && types[3] == 'Z');

    MemDb db;
    SqlStore s([&](const std::string& sql, const std::vector<std::string>& p) { return db.exec(sql, p); });
    auto d = Json::mkobj();
    d->obj["v"] = Json::mknum(7);
    CHECK(s.put("things", "a", d));
    CHECK(s.exists("things", "a"));
    CHECK(!s.exists("things", "b"));
    JsonP g = s.get("things", "a");
    CHECK(g && (int)g->n("v") == 7);
    CHECK(s.get("things", "b") == nullptr);
    s.put("things", "b", d);
    CHECK(s.list("things").size() == 2);
    s.append("log", d);
    s.append("log", d);
    CHECK(s.read_log("log").size() == 2);
    bool saw_upsert = false;
    for (auto& q2 : db.seen_sql)
        if (q2.find("ON CONFLICT") != std::string::npos) saw_upsert = true;
    CHECK(saw_upsert);
    CHECK(!SqlStore::ddl().empty());

    std::string captured;
    std::string canned = framed('T', rd) + framed('D', dr) + framed('C', std::string("SELECT 1") + '\0') + framed('Z', "I");
    bool sent = false;
    Transport tr;
    tr.send = [&](const std::string& b) { captured += b; sent = true; return true; };
    int calls = 0;
    tr.recv = [&](std::string& out) {
        if (calls++ > 0) return false;
        out = canned;
        return true;
    };
    PgConn conn(tr);
    Rows r = conn.exec("SELECT body FROM documents WHERE coll=$1", {"things"});
    CHECK(sent);
    CHECK(captured.find('P') != std::string::npos);
    CHECK(captured.find('B') != std::string::npos);
    CHECK(r.size() == 1);
    CHECK(r[0].size() == 2 && r[0][0] == "hello");

    Transport ts;
    std::string startup_canned = framed('R', std::string(4, '\0')) + framed('Z', "I");
    int sc = 0;
    ts.send = [&](const std::string&) { return true; };
    ts.recv = [&](std::string& out) {
        if (sc++ > 0) return false;
        out = startup_canned;
        return true;
    };
    PgConn c2(ts);
    CHECK(c2.startup("fve", "fve"));
}
