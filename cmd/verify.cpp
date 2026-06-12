#include "../core/engine.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace fve;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: verify <program.json>\n";
        return 2;
    }
    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    Program p = load_program(ss.str());
    AuditChain chain;
    RunResult r = run(p, chain);
    std::cout << dump(r.report) << "\n";
    std::cout << "cert_valid: " << (validate(r.cert) ? "true" : "false") << "\n";
    std::cout << "audit_ok: " << (chain.verify() ? "true" : "false") << "\n";
    return r.verdict == Verdict::Safe ? 0 : r.verdict == Verdict::Fail ? 1 : 3;
}
