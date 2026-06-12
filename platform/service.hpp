#pragma once
#include "../audit/audit.hpp"
#include "../registry/registry.hpp"
#include "../casebase/casebase.hpp"
#include "../platform/event.hpp"
#include "../platform/incremental.hpp"
#include "../platform/identity.hpp"
#include "../core/criticality/criticality.hpp"
#include "../core/ast/json.hpp"
#include <mutex>
#include <string>
#include <vector>

namespace fve {

class Service {
public:
    explicit Service(const std::string& data_dir);

    std::string ingest(const std::string& name, const std::string& mime,
                       const std::vector<uint8_t>& bytes);
    JsonP analyze(const std::string& artifact_id);
    JsonP recognize(const std::string& artifact_id);
    JsonP verify(const std::string& artifact_id, double rel_dev = 0.05);
    JsonP get_certificate(const std::string& run_id) const;
    JsonP get_artifact_meta(const std::string& artifact_id) const;
    JsonP get_graph(const std::string& artifact_id);
    JsonP provenance(const std::string& id) const;
    JsonP audit_history(const std::string& subject = "") const;
    bool audit_ok() const;
    std::vector<std::string> artifacts() const;

    JsonP crawl(const std::string& root);
    JsonP folder_tree() const;
    JsonP registry_files(int folder_id = -1) const;
    JsonP verify_registered(const std::string& file_id, double rel_dev = 0.05);
    JsonP search(const std::string& query, int limit = 10) const;
    JsonP admissible_region(const std::string& artifact_id, double rel_dev = 0.3, double eps = 0.01);
    JsonP similar_cases(const std::string& artifact_id, double rel_dev = 0.05);
    JsonP diff(const std::string& run_a, const std::string& run_b);
    JsonP run_history(const std::string& artifact_id) const;
    JsonP handle_event(const ProductionEvent& ev);
    JsonP get_event(const std::string& id) const;
    JsonP list_events(int min_priority) const;
    JsonP list_work_orders() const;
    JsonP resolve_work_order(const std::string& wo_id, const std::string& fix_id, const std::map<std::string, Ival>& patch);
    JsonP production_dashboard() const;
    JsonP production_report(const ProductionEvent& ev);
    JsonP report_status(const std::string& event_id) const;
    JsonP section_status() const;
    JsonP build_section_map();
    JsonP resolve_hole(const std::string& hole_id, const std::string& sym, double lo, double hi, const std::string& user_id);
    bool users_empty() const;
    std::string audit_head() const;
    JsonP bootstrap_admin();
    JsonP apply_migrations(const std::string& dir);
    JsonP crawl_summary() const;
    bool registry_empty() const;
    std::vector<std::string> head_file_ids() const;
    JsonP cache_stats() const;
    JsonP create_user(const std::string& display_name, const std::string& role,
                      const std::string& external_id, const std::vector<std::string>& scope);
    JsonP list_users() const;
    JsonP sign(const std::string& user_id, const std::string& subject, const std::string& meaning, const JsonP& payload);
    JsonP signatures(const std::string& subject) const;
    JsonP approve_fix(const std::string& fix_id, const std::string& reviewer_user_id);
    JsonP config_snapshot() const;
    JsonP coverage(const std::string& artifact_id) const;
    JsonP coverage_sample(const std::string& reason, int n) const;
    JsonP unsupported_heatmap() const;

    Store& store() { return store_; }

private:
    std::string blob_path(const std::string& id) const;
    std::string ext_for(const std::string& name) const;

    Store store_;
    Registry reg_;
    Audit audit_;
    CaseBase cb_;
    EvalCache eval_cache_;
    Identity ident_;
    CritConfig crit_cfg_;
    std::string config_hash_;
    std::mutex mu_;
};

}
