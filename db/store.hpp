#pragma once
#include "../core/ast/json.hpp"
#include <mutex>
#include <string>
#include <vector>

namespace fve {

class Store {
public:
    explicit Store(const std::string& root);
    bool put(const std::string& coll, const std::string& id, const JsonP& doc);
    JsonP get(const std::string& coll, const std::string& id) const;
    bool exists(const std::string& coll, const std::string& id) const;
    std::vector<std::string> list(const std::string& coll) const;
    bool append(const std::string& log, const JsonP& entry);
    std::vector<JsonP> read_log(const std::string& log) const;
    const std::string& root() const { return root_; }

private:
    std::string path(const std::string& coll, const std::string& id) const;
    void ensure(const std::string& dir) const;
    std::string root_;
    mutable std::mutex mu_;
};

}
