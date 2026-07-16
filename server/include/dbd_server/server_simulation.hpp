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
CommandResult SetUnitAutomationRules(
    WorldState& world,
    dbd::Id controller_player_id,
    dbd::Id unit_id,
    const std::vector<dbd::AutomationRule>& automation_rules);
CommandResult SetSquadAutomationRules(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id squad_id,
    const std::vector<dbd::AutomationRule>& automation_rules);
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
CommandResult IssueFormationMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target);
CommandResult QueueMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target);
CommandResult ClearQueuedOrders(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueHarvestOrder(WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::Id resource_node_id);
CommandResult IssueLootOrder(WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::Id dropped_cargo_id);
CommandResult IssueReturnToStorageOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    dbd::Id unit_id,
    dbd::Id storage_site_id);
CommandResult IssueEmergencyDepositOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueDropCargoOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueGuardThreatenedRouteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueInterceptRouteThreatOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueHaulRouteOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    dbd::HaulRouteSourceKind source_kind,
    dbd::Id source_id,
    dbd::Id storage_site_id);
CommandResult DeliverCargoToStorage(WorldState& world, dbd::Id unit_id, dbd::Id storage_site_id);
CommandResult ProgressHaulRoutes(WorldState& world, float delta_seconds);
CommandResult RefreshRouteThreats(WorldState& world);
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
CommandResult IssueRetreatOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult IssueFocusFireOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& attacker_unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id);
CommandResult IssueInterceptUnitOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id target_unit_id);
CommandResult IssueScoutAreaOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& center, float radius);
CommandResult IssuePatrolRouteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& point_a, const dbd::Vec3& point_b);
CommandResult QueueScoutAreaOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& center, float radius);
CommandResult QueuePatrolRouteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& point_a, const dbd::Vec3& point_b);
CommandResult IssueInvestigateContactOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id);
CommandResult IssueGuardUnitOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& guard_unit_ids, dbd::Id target_unit_id);
CommandResult IssueGuardSiteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& guard_unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id);
CommandResult IssueRepairStructureOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id structure_id);
CommandResult IssueRepairConstructionSiteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id construction_site_id);
CommandResult IssueSupplyRepairOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id);
CommandResult CraftItem(WorldState& world, dbd::Id player_id, dbd::Id item_id, std::uint32_t amount);
CommandResult InstallItem(WorldState& world, dbd::Id controller_player_id, dbd::Id item_id, const dbd::Vec3& position);
CommandResult UseItem(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id item_id);
CommandResult IssueHoldPositionOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& position);
CommandResult IssueStopOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids);
CommandResult ResolvePermanentUnitDeath(WorldState& world, dbd::Id unit_id);
CommandResult ProgressGuardOrders(WorldState& world, float delta_seconds);
CommandResult ProgressRepairOrders(WorldState& world, float delta_seconds);
CommandResult ProgressUnitMovement(WorldState& world, float delta_seconds);
CommandResult ProgressCombat(WorldState& world, float delta_seconds);
CommandResult RefreshKnownContacts(WorldState& world);
CommandResult RefreshEconomicTelemetry(WorldState& world);
CommandResult ProcessEconomicSettlement(WorldState& world, float delta_seconds);
CommandResult EvaluateAutomation(WorldState& world, float delta_seconds);
float ComputeInsurancePayout(const dbd::InsurancePolicyState& policy, float realized_loss_value);

}  // namespace dbd_server
