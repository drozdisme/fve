#include "../platform/service.hpp"
#include "../core/ast/json.hpp"
#include <cstdio>
#include <iostream>

using namespace fve;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: find <word> [data_dir]\n"; return 2; }
    std::string word = argv[1];
    std::string data = argc > 2 ? argv[2] : "./data";
    Service svc(data);
    JsonP r = svc.search(word);
    JsonP newest = r->has("newest") ? r->at("newest") : nullptr;
    if (newest && newest->t == Json::Obj) {
        std::cout << "newest version for '" << word << "':\n";
        std::cout << "  " << newest->str("rel_path") << "\n";
        std::cout << "  format=" << newest->str("format_type")
                  << " domain=" << newest->str("domain")
                  << " revision=" << newest->str("revision") << "\n";
        std::cout << "  file_id=" << newest->str("id") << "\n";
    } else {
        std::cout << "no match for '" << word << "'\n";
    }
    std::cout << dump(r) << "\n";
    return 0;
}
