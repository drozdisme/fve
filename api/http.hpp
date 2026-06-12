#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fve {

struct Req {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::string body;
    std::string header(const std::string& k) const;
};

struct Res {
    int status = 200;
    std::string ctype = "application/json";
    std::string body;
    static Res json(const std::string& j, int st = 200);
    static Res text(const std::string& t, int st = 200, const std::string& ct = "text/plain");
};

using Handler = std::function<Res(const Req&)>;

std::string multipart_file(const std::string& body, const std::string& ctype, std::string& fname);
std::string url_decode(const std::string& s);

class Http {
public:
    void route(const std::string& method, const std::string& pattern, Handler h);
    void static_dir(const std::string& prefix, const std::string& dir);
    Res handle(Req& req) { return dispatch(req); }
    void serve_conn(int c);
    int listen(const std::string& host, int port);

private:
    struct R {
        std::string method;
        std::vector<std::string> segs;
        Handler h;
    };
    Res dispatch(Req& req);
    Res serve_file(const std::string& path);
    std::vector<R> routes_;
    std::map<std::string, std::string> static_;
};

}
