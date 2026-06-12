#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fve {

struct Xml;
using Xmlp = std::shared_ptr<Xml>;

struct Xml {
    std::string tag;
    std::map<std::string, std::string> attrs;
    std::string text;
    std::vector<Xmlp> kids;

    std::string attr(const std::string& k, const std::string& def = "") const;
    Xmlp first(const std::string& t) const;
    std::vector<Xmlp> all(const std::string& t) const;
    void find_all(const std::string& t, std::vector<Xmlp>& out) const;
};

Xmlp parse_xml(const std::string& src);
std::string local_name(const std::string& tag);

}
