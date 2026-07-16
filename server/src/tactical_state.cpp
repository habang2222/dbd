#include "dbd_server/tactical_state.hpp"

namespace dbd_server {

void MarkTacticalState(dbd::UnitState& unit, std::string_view state) {
    unit.tactical_state.assign(state);
}

void ClearQueueForTacticalOverride(dbd::UnitState& unit, std::string_view reason) {
    if (!unit.queued_orders.empty()) {
        unit.queue_interrupt_reason.assign(reason);
    } else {
        unit.queue_interrupt_reason.clear();
    }
    unit.queued_orders.clear();
    unit.tactical_state.assign(reason);
}

}  // namespace dbd_server
