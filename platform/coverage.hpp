#pragma once
#include "../db/store.hpp"
#include "../model/workbook/workbook.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

// Static coverage scan of a workbook: which cells the verifier can cover and,
// for those it cannot, a classified reason. This is an observability metric
// (separate from the verifier's runtime holes), computed at analyze() time.
enum class HoleReason { ParseError, ExternalRef, UnsupportedFunction, EmptyOrText, Unknown };

std::string hole_reason_name(HoleReason r);

struct CoverageSample {
    std::string cell_ref;
    std::string formula;
    std::string reason; // e.g. "unsupported_function:XLOOKUP"
};

struct CoverageLedger {
    int total_cells = 0;
    int formula_cells = 0;
    int constant_cells = 0;
    int hole_cells = 0;
    std::map<std::string, int> hole_reasons; // reason -> count
    std::string confidence;                  // high, medium, low
    std::vector<std::string> flags;
    std::vector<CoverageSample> samples;     // capped examples for the inspector
};

CoverageLedger compute_coverage(const Workbook& wb);

// Persist the ledger for a file and upsert the global unsupported-construct heatmap.
void persist_coverage(Store& store, const std::string& file_id, const CoverageLedger& cl);

}
