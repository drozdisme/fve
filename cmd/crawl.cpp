#include "../l0/registry.hpp"
#include "../core/ast/json.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

using namespace fve;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: crawl <root_dir> [data_dir] [--sql out.sql]\n";
        return 2;
    }
    std::string root = argv[1];
    std::string data = argc > 2 && std::string(argv[2]).substr(0, 2) != "--" ? argv[2] : "./data";
    std::string sql_out;
    for (int i = 2; i < argc; i++)
        if (std::string(argv[i]) == "--sql" && i + 1 < argc) sql_out = argv[i + 1];

    Catalog reg = build_registry(root);
    Store store(data);
    migrate(reg, store);

    if (!sql_out.empty()) {
        std::ofstream f(sql_out);
        f << registry_sql(reg);
    }

    std::map<std::string, int> fmt;
    int heads = 0;
    for (const auto& e : reg.files) {
        fmt[e.format_type]++;
        if (e.is_head) heads++;
    }
    auto rep = Json::mkobj();
    rep->obj["root"] = Json::mkstr(root);
    rep->obj["folders"] = Json::mknum((double)reg.folders.size());
    rep->obj["files"] = Json::mknum((double)reg.files.size());
    rep->obj["heads"] = Json::mknum(heads);
    auto byfmt = Json::mkobj();
    for (auto& kv : fmt) byfmt->obj[kv.first] = Json::mknum(kv.second);
    rep->obj["by_format"] = byfmt;
    std::cout << dump(rep) << "\n";
    return 0;
}
