#include "http.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <vector>

namespace fve {

std::string Req::header(const std::string& k) const {
    std::string lk;
    for (char c : k) lk += (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
    auto it = headers.find(lk);
    return it == headers.end() ? "" : it->second;
}

Res Res::json(const std::string& j, int st) { return {st, "application/json", j}; }
Res Res::text(const std::string& t, int st, const std::string& ct) { return {st, ct, t}; }

std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = std::stoi(s.substr(i + 1, 2), nullptr, 16);
            o += (char)v;
            i += 2;
        } else if (s[i] == '+') o += ' ';
        else o += s[i];
    }
    return o;
}

std::string multipart_file(const std::string& body, const std::string& ctype, std::string& fname) {
    size_t bp = ctype.find("boundary=");
    if (bp == std::string::npos) return "";
    std::string boundary = "--" + ctype.substr(bp + 9);
    size_t pos = body.find(boundary);
    while (pos != std::string::npos) {
        size_t hs = pos + boundary.size();
        size_t he = body.find("\r\n\r\n", hs);
        if (he == std::string::npos) break;
        std::string head = body.substr(hs, he - hs);
        size_t ds = he + 4;
        size_t de = body.find(boundary, ds);
        if (de == std::string::npos) break;
        size_t dend = de;
        if (dend >= 2 && body[dend - 2] == '\r') dend -= 2;
        if (head.find("filename=\"") != std::string::npos) {
            size_t fs = head.find("filename=\"") + 10;
            size_t fe = head.find('"', fs);
            fname = head.substr(fs, fe - fs);
            return body.substr(ds, dend - ds);
        }
        pos = de;
    }
    return "";
}

void Http::route(const std::string& method, const std::string& pattern, Handler h) {
    R r;
    r.method = method;
    r.h = h;
    std::stringstream ss(pattern);
    std::string seg;
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) r.segs.push_back(seg);
    routes_.push_back(r);
}

void Http::static_dir(const std::string& prefix, const std::string& dir) { static_[prefix] = dir; }

Res Http::serve_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Res::text("not found", 404);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string ct = "text/plain";
    if (path.size() > 5 && path.substr(path.size() - 5) == ".html") ct = "text/html";
    else if (path.size() > 3 && path.substr(path.size() - 3) == ".js") ct = "application/javascript";
    else if (path.size() > 4 && path.substr(path.size() - 4) == ".css") ct = "text/css";
    return Res::text(ss.str(), 200, ct);
}

Res Http::dispatch(Req& req) {
    std::vector<std::string> segs;
    std::stringstream ss(req.path);
    std::string seg;
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) segs.push_back(seg);

    for (const auto& r : routes_) {
        if (r.method != req.method) continue;
        if (r.segs.size() != segs.size()) continue;
        std::map<std::string, std::string> params;
        bool ok = true;
        for (size_t i = 0; i < r.segs.size(); i++) {
            if (!r.segs[i].empty() && r.segs[i][0] == ':') params[r.segs[i].substr(1)] = segs[i];
            else if (r.segs[i] != segs[i]) { ok = false; break; }
        }
        if (ok) {
            req.params = params;
            return r.h(req);
        }
    }
    for (const auto& kv : static_) {
        if (req.path.compare(0, kv.first.size(), kv.first) == 0) {
            std::string rest = req.path.substr(kv.first.size());
            if (rest.empty()) rest = "index.html";
            if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
            if (rest.empty()) rest = "index.html";
            return serve_file(kv.second + "/" + rest);
        }
    }
    return Res::json("{\"error\":\"not found\"}", 404);
}

namespace {

bool recv_request(int c, Req& req) {
    std::string data;
    char buf[8192];
    size_t hdr_end = std::string::npos;
    while (hdr_end == std::string::npos) {
        ssize_t n = ::read(c, buf, sizeof(buf));
        if (n <= 0) return false;
        data.append(buf, n);
        hdr_end = data.find("\r\n\r\n");
        if (data.size() > 64 * 1024 * 1024) return false;
    }
    std::string head = data.substr(0, hdr_end);
    std::string body = data.substr(hdr_end + 4);
    std::stringstream ss(head);
    std::string line;
    std::getline(ss, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::stringstream rl(line);
    std::string url;
    rl >> req.method >> url;
    size_t q = url.find('?');
    if (q != std::string::npos) {
        std::string qs = url.substr(q + 1);
        req.path = url.substr(0, q);
        std::stringstream qss(qs);
        std::string kv;
        while (std::getline(qss, kv, '&')) {
            size_t eq = kv.find('=');
            if (eq != std::string::npos) req.query[kv.substr(0, eq)] = url_decode(kv.substr(eq + 1));
        }
    } else req.path = url;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t c2 = line.find(':');
        if (c2 == std::string::npos) continue;
        std::string k = line.substr(0, c2), v = line.substr(c2 + 1);
        while (!v.empty() && v[0] == ' ') v.erase(0, 1);
        for (char& ch : k) if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        req.headers[k] = v;
    }
    size_t clen = 0;
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) clen = std::stoul(it->second);
    while (body.size() < clen) {
        ssize_t n = ::read(c, buf, sizeof(buf));
        if (n <= 0) break;
        body.append(buf, n);
    }
    req.body = body;
    return true;
}

void send_response(int c, const Res& res) {
    std::stringstream h;
    h << "HTTP/1.1 " << res.status << " OK\r\n";
    h << "Content-Type: " << res.ctype << "\r\n";
    h << "Access-Control-Allow-Origin: *\r\n";
    h << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n";
    h << "Access-Control-Allow-Headers: *\r\n";
    h << "Content-Length: " << res.body.size() << "\r\n";
    h << "Connection: close\r\n\r\n";
    std::string head = h.str();
    ssize_t w1 = ::write(c, head.data(), head.size());
    ssize_t w2 = ::write(c, res.body.data(), res.body.size());
    (void)w1;
    (void)w2;
}

}

int Http::listen(const std::string& host, int port) {
    ::signal(SIGPIPE, SIG_IGN);
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = host == "0.0.0.0" ? htonl(INADDR_ANY) : inet_addr(host.c_str());
    if (::bind(s, (sockaddr*)&a, sizeof(a)) < 0) { ::perror("bind"); return 1; }
    if (::listen(s, 128) < 0) { ::perror("listen"); return 1; }
    std::printf("fve api on %s:%d\n", host.c_str(), port);
    std::fflush(stdout);

    std::queue<int> q;
    std::mutex qm;
    std::condition_variable cv;
    unsigned hw = std::thread::hardware_concurrency();
    int workers = hw ? (int)hw * 2 : 8;
    std::vector<std::thread> pool;
    for (int i = 0; i < workers; i++)
        pool.emplace_back([&] {
            while (true) {
                int c;
                {
                    std::unique_lock<std::mutex> lk(qm);
                    cv.wait(lk, [&] { return !q.empty(); });
                    c = q.front();
                    q.pop();
                }
                if (c < 0) break;
                serve_conn(c);
            }
        });

    while (true) {
        int c = ::accept(s, nullptr, nullptr);
        if (c < 0) continue;
        {
            std::lock_guard<std::mutex> lk(qm);
            q.push(c);
        }
        cv.notify_one();
    }
    return 0;
}

void Http::serve_conn(int c) {
    Req req;
    if (recv_request(c, req)) {
        if (req.method == "OPTIONS") send_response(c, Res::text("", 204));
        else send_response(c, dispatch(req));
    }
    ::close(c);
}

}
