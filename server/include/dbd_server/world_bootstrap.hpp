#pragma once

#include "dbd_server/world_state.hpp"

namespace dbd_server {

struct BootstrapResult {
    dbd::Id player_id {};
    dbd::Id lineage_id {};
    dbd::Id squad_id {};
    std::vector<dbd::Id> starter_unit_ids {};
    std::vector<dbd::Id> region_ids {};
    std::vector<dbd::Id> resource_node_ids {};
    dbd::Id depot_structure_id {};
    dbd::Id depot_storage_id {};
};

BootstrapResult BootstrapSinglePlayerWarSlice(WorldState& world, const char* player_name);

}  // namespace dbd_server
