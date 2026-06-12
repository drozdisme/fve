#pragma once
#include "../platform/service.hpp"
#include <string>

namespace fve {

bool grpc_unframe(const std::string& body, std::string& msg);
std::string grpc_response(const std::string& reply, int status);

class Grpc {
public:
    explicit Grpc(Service& s) : svc_(s) {}
    std::string dispatch(const std::string& method, const std::string& msg, int& status);

private:
    Service& svc_;
};

}
