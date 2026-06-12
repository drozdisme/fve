#include "../platform/service.hpp"
#include "../l0/watcher.hpp"
#include <iostream>

using namespace fve;

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: watch <root> <data_dir> [interval_ms]\n"; return 2; }
    std::string root = argv[1], data = argv[2];
    int interval = argc > 3 ? std::atoi(argv[3]) : 2000;
    Service svc(data);
    FolderWatcher w(root, svc, interval);
    std::cerr << "watching " << root << " (interval " << interval << "ms)\n";
    w.run();
    return 0;
}
