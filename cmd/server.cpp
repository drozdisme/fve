#include "../api/http.hpp"
#include "../api/routes.hpp"
#include "../platform/service.hpp"
#include <cstdlib>
#include <string>

using namespace fve;

int main(int argc, char** argv) {
    std::string data = argc > 1 ? argv[1] : "./data";
    std::string ui = argc > 2 ? argv[2] : "./ui";
    int port = argc > 3 ? std::atoi(argv[3]) : 8080;

    Service svc(data);
    Http http;
    register_routes(http, svc);
    http.static_dir("/", ui);
    return http.listen("0.0.0.0", port);
}
