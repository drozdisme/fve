#pragma once
#include <string>
#include <vector>

namespace fve {

struct DomainSchema {
    std::string format_id;
    std::string domain;
    std::vector<std::string> markers;
};

const std::vector<DomainSchema>& schema_registry();

struct Fingerprint {
    std::string format_type = "unknown_format";
    std::string domain = "unknown";
    std::string structural_sig;
    int marker_hits = 0;
    bool parsed = false;
    std::vector<std::string> markers;
};

struct QuickStamp {
    long mtime = 0;
    long size = 0;
};

struct FullStamp {
    QuickStamp quick;
    std::string sha256;
};

QuickStamp quick_stamp(const std::string& path);
FullStamp full_stamp(const std::string& path);

Fingerprint fingerprint(const std::string& path, const std::string& ext);
Fingerprint fingerprint_text(const std::string& head, const std::string& ext);

}
