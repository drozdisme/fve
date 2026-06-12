#include "version.hpp"
#include <cctype>
#include <vector>

namespace fve {

namespace {

std::string strip_ext(const std::string& n) {
    size_t d = n.rfind('.');
    return d == std::string::npos ? n : n.substr(0, d);
}

bool is_sep(char c) { return c == '_' || c == '-' || c == ' ' || c == '.' || c == '(' || c == ')'; }

std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (is_sep(c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string lower(const std::string& s) {
    std::string o = s;
    for (char& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}

bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

[[maybe_unused]] static bool all_alpha(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isalpha((unsigned char)c)) return false;
    return true;
}

double letters_rank(const std::string& s) {
    double r = 0;
    for (char c : s) r = r * 26 + (std::tolower((unsigned char)c) - 'a' + 1);
    return r;
}

bool has_digit(const std::string& s) {
    for (char c : s) if (std::isdigit((unsigned char)c)) return true;
    return false;
}

bool is_alnum(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isalnum((unsigned char)c)) return false;
    return true;
}

// "g1" -> letter G major (7) + minor .001 ; "d" -> 4 ; "12" -> 12
double alnum_rank(const std::string& s) {
    std::string alpha, digits;
    for (char c : s) {
        if (std::isalpha((unsigned char)c)) alpha += c;
        else if (std::isdigit((unsigned char)c)) digits += c;
    }
    if (!alpha.empty()) {
        double r = letters_rank(alpha);
        if (!digits.empty()) r += std::stod(digits) / 1000.0;
        return r;
    }
    return digits.empty() ? 0.0 : std::stod(digits);
}

bool rev_value(const std::string& s) {
    return is_alnum(s) && s.size() <= 4 && (has_digit(s) || s.size() <= 2);
}

}

Rev parse_revision(const std::string& filename) {
    Rev rev;
    std::string base = strip_ext(filename);
    auto toks = tokenize(base);
    std::vector<std::string> keep;
    int rev_at = -1;

    for (size_t i = 0; i < toks.size(); i++) {
        std::string t = lower(toks[i]);
        if ((t == "rev" || t == "revision" || t == "ver" || t == "version") && i + 1 < toks.size()) {
            std::string nx = lower(toks[i + 1]);
            if (rev_value(nx)) { rev.rank = alnum_rank(nx); rev.label = "rev" + nx; rev.found = true; rev_at = (int)i; i++; continue; }
        }
        if (t.size() > 3 && t.compare(0, 3, "rev") == 0) {
            std::string rest = t.substr(3);
            if (rev_value(rest)) { rev.rank = alnum_rank(rest); rev.label = "rev" + rest; rev.found = true; rev_at = (int)i; continue; }
        }
        if ((t[0] == 'v') && t.size() > 1 && all_digits(t.substr(1))) {
            rev.rank = std::stod(t.substr(1)); rev.label = "v" + t.substr(1); rev.found = true; rev_at = (int)i; continue;
        }
        if (t[0] == 'r' && t.size() > 1 && all_digits(t.substr(1))) {
            rev.rank = std::stod(t.substr(1)); rev.label = "r" + t.substr(1); rev.found = true; rev_at = (int)i; continue;
        }
        keep.push_back(t);
    }
    (void)rev_at;

    std::string key;
    for (size_t i = 0; i < keep.size(); i++) {
        if (i) key += "_";
        key += keep[i];
    }
    if (key.empty()) key = lower(base);
    rev.config_key = key;
    return rev;
}

}
