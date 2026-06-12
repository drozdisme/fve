#include "formula.hpp"
#include <cctype>
#include <cstdlib>

namespace fve {

namespace {

enum class T { Num, Str, Ident, Ref, Op, LP, RP, Comma, End };

struct Tok {
    T t;
    std::string s;
    double num = 0.0;
};

struct Lexer {
    const std::string& s;
    size_t i = 0;
    explicit Lexer(const std::string& src) : s(src) {}

    void skip() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n')) i++;
    }

    bool is_ref_start(size_t p) {
        return p < s.size() && (s[p] == '$' || std::isalpha((unsigned char)s[p]));
    }

    Tok next() {
        skip();
        if (i >= s.size()) return {T::End, ""};
        char c = s[i];
        if (c == '(') { i++; return {T::LP, "("}; }
        if (c == ')') { i++; return {T::RP, ")"}; }
        if (c == ',') { i++; return {T::Comma, ","}; }
        if (c == '"') return string_tok();
        if (c == '\'' ) return sheet_ref();
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1])))
            return number();
        if (std::isalpha((unsigned char)c) || c == '$' || c == '_') return ident_or_ref();
        return op();
    }

    Tok string_tok() {
        std::string out;
        i++;
        while (i < s.size()) {
            if (s[i] == '"') {
                if (i + 1 < s.size() && s[i + 1] == '"') { out += '"'; i += 2; continue; }
                i++;
                break;
            }
            out += s[i++];
        }
        return {T::Str, out};
    }

    Tok number() {
        size_t st = i;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) i++;
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
        }
        Tok t{T::Num, s.substr(st, i - st)};
        t.num = std::strtod(t.s.c_str(), nullptr);
        return t;
    }

    Tok sheet_ref() {
        std::string out;
        out += '\'';
        i++;
        while (i < s.size() && s[i] != '\'') out += s[i++];
        if (i < s.size()) i++;
        out += '\'';
        if (i < s.size() && s[i] == '!') {
            out += '!';
            i++;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '$' || s[i] == ':')) out += s[i++];
        }
        return {T::Ref, out};
    }

    Tok ident_or_ref() {
        size_t st = i;
        std::string word;
        while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '$' || s[i] == '_' || s[i] == '.')) word += s[i++];
        if (i < s.size() && s[i] == '!') {
            word += '!';
            i++;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '$' || s[i] == ':')) word += s[i++];
            return {T::Ref, word};
        }
        if (looks_ref(word) && i < s.size() && s[i] == ':' && is_ref_start(i + 1)) {
            word += ':';
            i++;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '$')) word += s[i++];
            return {T::Ref, word};
        }
        size_t save = i;
        skip();
        if (i < s.size() && s[i] == '(') return {T::Ident, word};
        i = save;
        if (looks_ref(word)) return {T::Ref, word};
        (void)st;
        return {T::Ident, word};
    }

    bool looks_ref(const std::string& w) {
        size_t p = 0;
        if (p < w.size() && w[p] == '$') p++;
        size_t letters = 0;
        while (p < w.size() && std::isalpha((unsigned char)w[p])) { p++; letters++; }
        if (p < w.size() && w[p] == '$') p++;
        size_t digits = 0;
        while (p < w.size() && std::isdigit((unsigned char)w[p])) { p++; digits++; }
        return letters > 0 && digits > 0 && p == w.size();
    }

    Tok op() {
        char c = s[i];
        if ((c == '<' || c == '>') && i + 1 < s.size() && s[i + 1] == '=') {
            i += 2;
            return {T::Op, std::string(1, c) + "="};
        }
        if (c == '<' && i + 1 < s.size() && s[i + 1] == '>') {
            i += 2;
            return {T::Op, "<>"};
        }
        i++;
        return {T::Op, std::string(1, c)};
    }
};

void split_sheet(const std::string& raw, std::string& sheet, std::string& body) {
    size_t bang = raw.rfind('!');
    if (bang == std::string::npos) { sheet = ""; body = raw; return; }
    sheet = raw.substr(0, bang);
    if (!sheet.empty() && sheet.front() == '\'' && sheet.back() == '\'')
        sheet = sheet.substr(1, sheet.size() - 2);
    body = raw.substr(bang + 1);
}

XAstp make_ref(const std::string& raw) {
    auto a = std::make_shared<XAst>();
    std::string sheet, body;
    split_sheet(raw, sheet, body);
    size_t colon = body.find(':');
    if (colon != std::string::npos) {
        a->kind = XKind::Range;
        a->sheet = sheet;
        int r, c;
        bool ar, ac;
        parse_a1(body.substr(0, colon), a->ref.row, a->ref.col, a->ref.abs_row, a->ref.abs_col);
        if (parse_a1(body.substr(colon + 1), r, c, ar, ac)) {
            a->ref_end.row = r;
            a->ref_end.col = c;
        }
        return a;
    }
    int r, c;
    bool ar, ac;
    if (parse_a1(body, r, c, ar, ac)) {
        a->kind = XKind::Cell;
        a->sheet = sheet;
        a->ref.row = r;
        a->ref.col = c;
        a->ref.abs_row = ar;
        a->ref.abs_col = ac;
        return a;
    }
    a->kind = XKind::NameRef;
    a->name = body;
    return a;
}

struct Parser {
    Lexer lex;
    Tok cur;
    bool ok = true;
    std::string err;
    explicit Parser(const std::string& s) : lex(s) { cur = lex.next(); }

    void adv() { cur = lex.next(); }
    void fail(const std::string& m) { if (ok) { ok = false; err = m; } }

    int prec(const std::string& o) {
        if (o == "=" || o == "<>" || o == "<" || o == ">" || o == "<=" || o == ">=") return 1;
        if (o == "&") return 2;
        if (o == "+" || o == "-") return 3;
        if (o == "*" || o == "/") return 4;
        if (o == "^") return 5;
        return 0;
    }

    XAstp parse_expr(int min_prec) {
        XAstp lhs = parse_unary();
        while (ok && cur.t == T::Op && prec(cur.s) >= min_prec && prec(cur.s) > 0) {
            std::string o = cur.s;
            int p = prec(o);
            adv();
            int nxt = (o == "^") ? p : p + 1;
            XAstp rhs = parse_expr(nxt);
            auto b = std::make_shared<XAst>();
            b->kind = XKind::Bin;
            b->op = o;
            b->args = {lhs, rhs};
            lhs = b;
        }
        return lhs;
    }

    XAstp parse_unary() {
        if (cur.t == T::Op && (cur.s == "-" || cur.s == "+")) {
            std::string o = cur.s;
            adv();
            auto u = std::make_shared<XAst>();
            u->kind = XKind::Un;
            u->op = o;
            u->args = {parse_unary()};
            return u;
        }
        return parse_postfix();
    }

    XAstp parse_postfix() {
        XAstp p = parse_primary();
        while (ok && cur.t == T::Op && cur.s == "%") {
            adv();
            auto u = std::make_shared<XAst>();
            u->kind = XKind::Un;
            u->op = "%";
            u->args = {p};
            p = u;
        }
        return p;
    }

    XAstp parse_primary() {
        if (cur.t == T::Num) {
            auto a = std::make_shared<XAst>();
            a->kind = XKind::Num;
            a->num = cur.num;
            adv();
            return a;
        }
        if (cur.t == T::Str) {
            auto a = std::make_shared<XAst>();
            a->kind = XKind::Str;
            a->str = cur.s;
            adv();
            return a;
        }
        if (cur.t == T::Ref) {
            auto a = make_ref(cur.s);
            adv();
            return a;
        }
        if (cur.t == T::LP) {
            adv();
            XAstp e = parse_expr(1);
            if (cur.t == T::RP) adv(); else fail("expected )");
            return e;
        }
        if (cur.t == T::Ident) {
            std::string name = cur.s;
            adv();
            if (cur.t == T::LP) {
                adv();
                auto call = std::make_shared<XAst>();
                call->kind = XKind::Call;
                call->fn = upper(name);
                if (cur.t != T::RP) {
                    call->args.push_back(parse_expr(1));
                    while (cur.t == T::Comma) {
                        adv();
                        call->args.push_back(parse_expr(1));
                    }
                }
                if (cur.t == T::RP) adv(); else fail("expected ) in call");
                return call;
            }
            auto a = std::make_shared<XAst>();
            a->kind = XKind::NameRef;
            a->name = name;
            return a;
        }
        fail("unexpected token");
        return std::make_shared<XAst>();
    }

    static std::string upper(const std::string& s) {
        std::string o = s;
        for (char& c : o) if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        return o;
    }
};

}

XParse parse_formula(const std::string& src) {
    std::string body = src;
    if (!body.empty() && body[0] == '=') body = body.substr(1);
    XParse out;
    if (body.empty()) {
        out.ok = false;
        out.err = "empty";
        return out;
    }
    Parser p(body);
    out.ast = p.parse_expr(1);
    out.ok = p.ok && p.cur.t == T::End;
    if (!out.ok && out.err.empty()) out.err = p.err.empty() ? "trailing tokens" : p.err;
    return out;
}

void collect_refs(const XAstp& a, std::vector<Ref>& cells,
                  std::vector<std::pair<Ref, Ref>>& ranges,
                  std::vector<std::string>& names) {
    if (!a) return;
    switch (a->kind) {
        case XKind::Cell: cells.push_back(a->ref); break;
        case XKind::Range: ranges.push_back({a->ref, a->ref_end}); break;
        case XKind::NameRef: names.push_back(a->name); break;
        default: break;
    }
    for (const auto& c : a->args) collect_refs(c, cells, ranges, names);
}

std::string xast_str(const XAstp& a) {
    if (!a) return "";
    switch (a->kind) {
        case XKind::Num: return std::to_string(a->num);
        case XKind::Str: return "\"" + a->str + "\"";
        case XKind::Cell: return (a->sheet.empty() ? "" : a->sheet + "!") + a1(a->ref.row, a->ref.col);
        case XKind::Range:
            return (a->sheet.empty() ? "" : a->sheet + "!") + a1(a->ref.row, a->ref.col) + ":" + a1(a->ref_end.row, a->ref_end.col);
        case XKind::NameRef: return a->name;
        case XKind::Un: return a->op + xast_str(a->args[0]);
        case XKind::Bin: return "(" + xast_str(a->args[0]) + a->op + xast_str(a->args[1]) + ")";
        case XKind::Call: {
            std::string s = a->fn + "(";
            for (size_t i = 0; i < a->args.size(); i++) {
                if (i) s += ",";
                s += xast_str(a->args[i]);
            }
            return s + ")";
        }
    }
    return "";
}

}
