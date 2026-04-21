#pragma once

#include "dbd_server/world_state.hpp"

#include <string>

namespace dbd_server {

struct CommandResult {
    bool ok {false};
    std::string message {};
    dbd::Id entity_id {};
};

CommandResult AssignUnitsToSquad(WorldState& world, dbd::Id squad_id, const std::vector<dbd::Id>& unit_ids);
CommandResult SetSquadStance(WorldState& world, dbd::Id owner_player_id, dbd::Id squad_id, dbd::SquadStance stance);
CommandResult GrantAuthority(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id target_player_id,
    dbd::AuthorityScopeType scope_type,
    const std::vector<dbd::Id>& target_ids,
    std::uint32_t permission_mask,
    bool revocable,
    std::uint64_t expires_at_utc_ms);
CommandResult IssueMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target);
CommandResult IssueHarvestOrder(WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::Id resource_node_id);
CommandResult DeliverCargoToStorage(WorldState& world, dbd::Id unit_id, dbd::Id storage_site_id);
CommandResult StartFlattenJob(WorldState& world, dbd::Id player_id, const dbd::Vec3& center, float radius, float target_grade);
CommandResult AssignUnitsToFlattenJob(WorldState& world, dbd::Id controller_player_id, dbd::Id flatten_job_id, const std::vector<dbd::Id>& unit_ids);
CommandResult RemoveUnitsFromFlattenJob(WorldState& world, dbd::Id controller_player_id, dbd::Id flatten_job_id, const std::vector<dbd::Id>& unit_ids);
CommandResult ProgressFlattenJobs(WorldState& world, float delta_seconds);
CommandResult StartConstruction(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::StructureType structure_type,
    const dbd::Vec3& position,
    float footprint_radius);
CommandResult AssignUnitsToConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids);
CommandResult RemoveUnitsFromConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids);
CommandResult ProgressConstructionSites(WorldState& world, float delta_seconds);
CommandResult DamageConstructionSite(WorldState& world, dbd::Id attacker_player_id, dbd::Id construction_site_id, float damage);
CommandResult ResolveAttack(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& attacker_unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id);
CommandResult ResolvePermanentUnitDeath(WorldState& world, dbd::Id unit_id);
CommandResult ProgressUnitMovement(WorldState& world, float delta_seconds);
CommandResult ProgressCombat(WorldState& world, float delta_seconds);
CommandResult ProcessEconomicSettlement(WorldState& world, float delta_seconds);
CommandResult EvaluateAutomation(WorldState& world, float delta_seconds);
float ComputeInsurancePayout(const dbd::InsurancePolicyState& policy, float realized_loss_value);

}  // namespace dbd_server
