#include "pgwire.hpp"
#include <cstdint>
#include <cstring>

namespace fve {

namespace {

void put_i32(std::string& s, uint32_t v) {
    s += (char)((v >> 24) & 0xFF);
    s += (char)((v >> 16) & 0xFF);
    s += (char)((v >> 8) & 0xFF);
    s += (char)(v & 0xFF);
}

void put_i16(std::string& s, uint16_t v) {
    s += (char)((v >> 8) & 0xFF);
    s += (char)(v & 0xFF);
}

uint32_t get_i32(const std::string& s, size_t off) {
    return ((uint32_t)(uint8_t)s[off] << 24) | ((uint32_t)(uint8_t)s[off + 1] << 16) |
           ((uint32_t)(uint8_t)s[off + 2] << 8) | (uint8_t)s[off + 3];
}

uint16_t get_i16(const std::string& s, size_t off) {
    return ((uint16_t)(uint8_t)s[off] << 8) | (uint8_t)s[off + 1];
}

std::string framed(char type, const std::string& payload) {
    std::string s;
    s += type;
    put_i32(s, (uint32_t)(payload.size() + 4));
    s += payload;
    return s;
}

}

std::string pg_startup(const std::string& user, const std::string& db) {
    std::string p;
    put_i32(p, 196608);
    p += "user";
    p += '\0';
    p += user;
    p += '\0';
    p += "database";
    p += '\0';
    p += db;
    p += '\0';
    p += '\0';
    std::string s;
    put_i32(s, (uint32_t)(p.size() + 4));
    s += p;
    return s;
}

std::string pg_query(const std::string& sql) {
    std::string p = sql;
    p += '\0';
    return framed('Q', p);
}

std::string pg_parse(const std::string& name, const std::string& sql) {
    std::string p = name;
    p += '\0';
    p += sql;
    p += '\0';
    put_i16(p, 0);
    return framed('P', p);
}

std::string pg_bind(const std::string& portal, const std::string& stmt,
                    const std::vector<std::string>& params) {
    std::string p = portal;
    p += '\0';
    p += stmt;
    p += '\0';
    put_i16(p, 0);
    put_i16(p, (uint16_t)params.size());
    for (const auto& v : params) {
        put_i32(p, (uint32_t)v.size());
        p += v;
    }
    put_i16(p, 0);
    return framed('B', p);
}

std::string pg_describe_portal(const std::string& portal) {
    std::string p;
    p += 'P';
    p += portal;
    p += '\0';
    return framed('D', p);
}

std::string pg_execute(const std::string& portal, int max_rows) {
    std::string p = portal;
    p += '\0';
    put_i32(p, (uint32_t)max_rows);
    return framed('E', p);
}

std::string pg_sync() { return framed('S', ""); }
std::string pg_terminate() { return framed('X', ""); }

bool pg_read_msg(const std::string& buf, size_t& off, PgMsg& msg) {
    if (off + 5 > buf.size()) return false;
    msg.type = buf[off];
    uint32_t len = get_i32(buf, off + 1);
    if (off + 1 + len > buf.size()) return false;
    msg.payload = buf.substr(off + 5, len - 4);
    off += 1 + len;
    return true;
}

std::vector<std::string> pg_row_description(const std::string& p) {
    std::vector<std::string> names;
    if (p.size() < 2) return names;
    uint16_t n = get_i16(p, 0);
    size_t i = 2;
    for (int k = 0; k < n && i < p.size(); k++) {
        size_t z = p.find('\0', i);
        if (z == std::string::npos) break;
        names.push_back(p.substr(i, z - i));
        i = z + 1 + 18;
    }
    return names;
}

std::vector<std::string> pg_data_row(const std::string& p) {
    std::vector<std::string> cols;
    if (p.size() < 2) return cols;
    uint16_t n = get_i16(p, 0);
    size_t i = 2;
    for (int k = 0; k < n && i + 4 <= p.size(); k++) {
        uint32_t len = get_i32(p, i);
        i += 4;
        if (len == 0xFFFFFFFF) {
            cols.push_back("");
        } else {
            cols.push_back(p.substr(i, len));
            i += len;
        }
    }
    return cols;
}

}
