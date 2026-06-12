#pragma once
#include "../core/ast/json.hpp"
#include <map>
#include <string>

namespace fve {

struct ProductionEvent {
    std::string id;
    std::string part_id;
    std::string section;
    std::string defect_type;
    std::map<std::string, double> actual_params;
    std::string operator_id;
    std::string voice_note_id;
    std::string photo_id;
    long timestamp = 0;
};

enum class EventAction { AutoFix, WorkOrder, Halt };

inline std::string action_name(EventAction a) {
    switch (a) {
        case EventAction::AutoFix: return "auto_fix";
        case EventAction::Halt: return "halt";
        default: return "work_order";
    }
}

ProductionEvent parse_event(const JsonP& body);

}
