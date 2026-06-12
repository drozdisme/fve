#include "xml.hpp"

namespace fve {

namespace {

std::string unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i);
            if (semi != std::string::npos) {
                std::string e = s.substr(i + 1, semi - i - 1);
                if (e == "lt") o += '<';
                else if (e == "gt") o += '>';
                else if (e == "amp") o += '&';
                else if (e == "quot") o += '"';
                else if (e == "apos") o += '\'';
                else if (!e.empty() && e[0] == '#') {
                    int code = e[1] == 'x' ? (int)strtol(e.c_str() + 2, nullptr, 16)
                                           : atoi(e.c_str() + 1);
                    if (code < 128) o += (char)code;
                    else o += '?';
                } else o += '?';
                i = semi;
                continue;
            }
        }
        o += s[i];
    }
    return o;
}

struct P {
    const std::string& s;
    size_t i = 0;
    explicit P(const std::string& src) : s(src) {}

    void skip_ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) i++;
    }

    void skip_misc() {
        while (i < s.size()) {
            skip_ws();
            if (i + 1 < s.size() && s[i] == '<' && (s[i + 1] == '?' )) {
                size_t e = s.find("?>", i);
                i = (e == std::string::npos) ? s.size() : e + 2;
            } else if (i + 3 < s.size() && s.compare(i, 4, "<!--") == 0) {
                size_t e = s.find("-->", i);
                i = (e == std::string::npos) ? s.size() : e + 3;
            } else if (i + 1 < s.size() && s[i] == '<' && s[i + 1] == '!') {
                size_t e = s.find('>', i);
                i = (e == std::string::npos) ? s.size() : e + 1;
            } else break;
        }
    }

    Xmlp element() {
        skip_misc();
        if (i >= s.size() || s[i] != '<') return nullptr;
        i++;
        auto node = std::make_shared<Xml>();
        while (i < s.size() && s[i] != ' ' && s[i] != '>' && s[i] != '/' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
            node->tag += s[i++];
        while (true) {
            skip_ws();
            if (i >= s.size()) break;
            if (s[i] == '/') {
                i++;
                if (i < s.size() && s[i] == '>') i++;
                return node;
            }
            if (s[i] == '>') { i++; break; }
            std::string key;
            while (i < s.size() && s[i] != '=' && s[i] != ' ' && s[i] != '>' && s[i] != '/') key += s[i++];
            skip_ws();
            std::string val;
            if (i < s.size() && s[i] == '=') {
                i++;
                skip_ws();
                char q = s[i];
                if (q == '"' || q == '\'') {
                    i++;
                    while (i < s.size() && s[i] != q) val += s[i++];
                    if (i < s.size()) i++;
                }
            }
            node->attrs[local_name(key)] = unescape(val);
        }
        std::string txt;
        while (i < s.size()) {
            if (s[i] == '<') {
                if (i + 1 < s.size() && s[i + 1] == '/') {
                    size_t e = s.find('>', i);
                    i = (e == std::string::npos) ? s.size() : e + 1;
                    break;
                }
                if (i + 8 < s.size() && s.compare(i, 9, "<![CDATA[") == 0) {
                    size_t e = s.find("]]>", i);
                    txt += s.substr(i + 9, (e == std::string::npos ? s.size() : e) - i - 9);
                    i = (e == std::string::npos) ? s.size() : e + 3;
                    continue;
                }
                if (i + 3 < s.size() && s.compare(i, 4, "<!--") == 0) {
                    size_t e = s.find("-->", i);
                    i = (e == std::string::npos) ? s.size() : e + 3;
                    continue;
                }
                Xmlp kid = element();
                if (kid) node->kids.push_back(kid);
            } else {
                txt += s[i++];
            }
        }
        node->text = unescape(txt);
        return node;
    }
};

}

std::string local_name(const std::string& tag) {
    size_t c = tag.find(':');
    return c == std::string::npos ? tag : tag.substr(c + 1);
}

std::string Xml::attr(const std::string& k, const std::string& def) const {
    auto it = attrs.find(k);
    return it == attrs.end() ? def : it->second;
}

Xmlp Xml::first(const std::string& t) const {
    for (const auto& k : kids)
        if (local_name(k->tag) == t) return k;
    return nullptr;
}

std::vector<Xmlp> Xml::all(const std::string& t) const {
    std::vector<Xmlp> v;
    for (const auto& k : kids)
        if (local_name(k->tag) == t) v.push_back(k);
    return v;
}

void Xml::find_all(const std::string& t, std::vector<Xmlp>& out) const {
    for (const auto& k : kids) {
        if (local_name(k->tag) == t) out.push_back(k);
        k->find_all(t, out);
    }
}

Xmlp parse_xml(const std::string& src) {
    P p(src);
    return p.element();
}

}
