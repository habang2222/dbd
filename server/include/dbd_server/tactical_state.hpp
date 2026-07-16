#pragma once

#include "dbd/domain_types.hpp"

#include <string_view>

namespace dbd_server {

// Centralize tactical text mutation while snapshots still expose legacy strings.
void MarkTacticalState(dbd::UnitState& unit, std::string_view state);
void ClearQueueForTacticalOverride(dbd::UnitState& unit, std::string_view reason);

}  // namespace dbd_server
