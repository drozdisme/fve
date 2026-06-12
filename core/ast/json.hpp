#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fve {

struct Json;
using JsonP = std::shared_ptr<Json>;

struct Json {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double num = 0.0;
    std::string s;
    std::vector<JsonP> arr;
    std::map<std::string, JsonP> obj;

    static JsonP mknull();
    static JsonP mkbool(bool v);
    static JsonP mknum(double v);
    static JsonP mkstr(const std::string& v);
    static JsonP mkarr();
    static JsonP mkobj();

    bool has(const std::string& k) const;
    JsonP at(const std::string& k) const;
    double n(const std::string& k, double def = 0.0) const;
    std::string str(const std::string& k, const std::string& def = "") const;
};

std::string dump(const JsonP& j, bool sorted = true);
JsonP parse(const std::string& text);

}
