#pragma once
#include "../api/http.hpp"
#include "../platform/service.hpp"

namespace fve {
void register_routes(Http& http, Service& svc);
}
