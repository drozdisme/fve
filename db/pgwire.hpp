#pragma once
#include <string>
#include <vector>

namespace fve {

std::string pg_startup(const std::string& user, const std::string& db);
std::string pg_query(const std::string& sql);
std::string pg_parse(const std::string& name, const std::string& sql);
std::string pg_bind(const std::string& portal, const std::string& stmt,
                    const std::vector<std::string>& params);
std::string pg_describe_portal(const std::string& portal);
std::string pg_execute(const std::string& portal, int max_rows = 0);
std::string pg_sync();
std::string pg_terminate();

struct PgMsg {
    char type = 0;
    std::string payload;
};

bool pg_read_msg(const std::string& buf, size_t& off, PgMsg& msg);
std::vector<std::string> pg_row_description(const std::string& payload);
std::vector<std::string> pg_data_row(const std::string& payload);

}
