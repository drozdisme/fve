#include "event.hpp"

namespace fve {

ProductionEvent parse_event(const JsonP& body) {
    ProductionEvent ev;
    if (!body) return ev;
    ev.part_id = body->str("part_id");
    ev.section = body->str("section");
    ev.defect_type = body->str("defect_type");
    ev.operator_id = body->str("operator_id");
    ev.voice_note_id = body->str("voice_note_id");
    ev.photo_id = body->str("photo_id");
    if (body->has("timestamp")) ev.timestamp = (long)body->n("timestamp");
    if (body->has("params") && body->at("params")->t == Json::Obj)
        for (const auto& kv : body->at("params")->obj)
            if (kv.second && kv.second->t == Json::Num) ev.actual_params[kv.first] = kv.second->num;
    return ev;
}

}
