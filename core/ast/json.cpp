#include "json.hpp"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace fve {

JsonP Json::mknull() { return std::make_shared<Json>(); }
JsonP Json::mkbool(bool v) {
    auto j = std::make_shared<Json>();
    j->t = Bool;
    j->b = v;
    return j;
}
JsonP Json::mknum(double v) {
    auto j = std::make_shared<Json>();
    j->t = Num;
    j->num = v;
    return j;
}
JsonP Json::mkstr(const std::string& v) {
    auto j = std::make_shared<Json>();
    j->t = Str;
    j->s = v;
    return j;
}
JsonP Json::mkarr() {
    auto j = std::make_shared<Json>();
    j->t = Arr;
    return j;
}
JsonP Json::mkobj() {
    auto j = std::make_shared<Json>();
    j->t = Obj;
    return j;
}

bool Json::has(const std::string& k) const { return obj.count(k) > 0; }
JsonP Json::at(const std::string& k) const {
    auto it = obj.find(k);
    return it == obj.end() ? mknull() : it->second;
}
double Json::n(const std::string& k, double def) const {
    auto it = obj.find(k);
    return (it == obj.end() || it->second->t != Num) ? def : it->second->num;
}
std::string Json::str(const std::string& k, const std::string& def) const {
    auto it = obj.find(k);
    return (it == obj.end() || it->second->t != Str) ? def : it->second->s;
}

namespace {

void esc(std::ostream& o, const std::string& s) {
    o << '"';
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\t': o << "\\t"; break;
            case '\r': o << "\\r"; break;
            default: o << c;
        }
    }
    o << '"';
}

void num_out(std::ostream& o, double v) {
    if (std::isnan(v)) { o << "\"nan\""; return; }
    if (std::isinf(v)) { o << (v < 0 ? "\"-inf\"" : "\"inf\""); return; }
    if (v == (long long)v && std::fabs(v) < 1e15) {
        o << (long long)v;
        return;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    o << buf;
}

void emit(std::ostream& o, const JsonP& j, bool sorted) {
    if (!j) { o << "null"; return; }
    switch (j->t) {
        case Json::Null: o << "null"; break;
        case Json::Bool: o << (j->b ? "true" : "false"); break;
        case Json::Num: num_out(o, j->num); break;
        case Json::Str: esc(o, j->s); break;
        case Json::Arr: {
            o << '[';
            for (size_t i = 0; i < j->arr.size(); i++) {
                if (i) o << ',';
                emit(o, j->arr[i], sorted);
            }
            o << ']';
            break;
        }
        case Json::Obj: {
            o << '{';
            bool first = true;
            for (const auto& kv : j->obj) {
                if (!first) o << ',';
                esc(o, kv.first);
                o << ':';
                emit(o, kv.second, sorted);
                first = false;
            }
            o << '}';
            break;
        }
    }
}

struct P {
    const std::string& s;
    size_t i = 0;
    explicit P(const std::string& t) : s(t) {}
    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) i++;
    }
    char peek() { return i < s.size() ? s[i] : '\0'; }
    JsonP value() {
        ws();
        char c = peek();
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return Json::mkstr(string());
        if (c == 't' || c == 'f') return boolean();
        if (c == 'n') { i += 4; return Json::mknull(); }
        return number();
    }
    JsonP object() {
        auto j = Json::mkobj();
        i++;
        ws();
        if (peek() == '}') { i++; return j; }
        while (true) {
            ws();
            std::string k = string();
            ws();
            i++;
            JsonP v = value();
            j->obj[k] = v;
            ws();
            if (peek() == ',') { i++; continue; }
            break;
        }
        ws();
        if (peek() == '}') i++;
        return j;
    }
    JsonP array() {
        auto j = Json::mkarr();
        i++;
        ws();
        if (peek() == ']') { i++; return j; }
        while (true) {
            j->arr.push_back(value());
            ws();
            if (peek() == ',') { i++; continue; }
            break;
        }
        ws();
        if (peek() == ']') i++;
        return j;
    }
    std::string string() {
        std::string out;
        i++;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    default: out += e;
                }
            } else {
                out += c;
            }
        }
        if (i < s.size()) i++;
        return out;
    }
    JsonP boolean() {
        if (s[i] == 't') { i += 4; return Json::mkbool(true); }
        i += 5;
        return Json::mkbool(false);
    }
    JsonP number() {
        if (peek() == '"') {
            std::string t = string();
            if (t == "inf") return Json::mknum(HUGE_VAL);
            if (t == "-inf") return Json::mknum(-HUGE_VAL);
            return Json::mknum(std::nan(""));
        }
        size_t st = i;
        while (i < s.size() &&
               (s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' ||
                s[i] == 'E' || (s[i] >= '0' && s[i] <= '9')))
            i++;
        return Json::mknum(std::stod(s.substr(st, i - st)));
    }
};

}

std::string dump(const JsonP& j, bool sorted) {
    std::ostringstream o;
    emit(o, j, sorted);
    return o.str();
}

JsonP parse(const std::string& text) {
    P p(text);
    return p.value();
}

}
