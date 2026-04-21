#include "dbd_server/world_bootstrap.hpp"

namespace dbd_server {

BootstrapResult BootstrapSinglePlayerWarSlice(WorldState& world, const char* player_name) {
    BootstrapResult result;

    auto& player = CreatePlayer(world, player_name);
    auto& lineage = CreateLineage(world, player.player_id, "Founding Lineage");
    auto& squad = CreateSquad(world, player.player_id, player.player_id, "Starter Warband");

    result.player_id = player.player_id;
    result.lineage_id = lineage.lineage_id;
    result.squad_id = squad.squad_id;

    constexpr std::uint32_t permissions =
        static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueMove) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueGather) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueBuild) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueAttack) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::AccessStorage) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::WithdrawResource) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::DepositResource) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::ManageInsurance) |
        static_cast<std::uint32_t>(dbd::AuthorityPermission::AssignMembers);

    CreateAuthority(world, player.player_id, player.player_id, dbd::AuthorityScopeType::LineageWide, lineage.lineage_id, permissions);

    auto& low = CreateRegionRiskProfile(world, dbd::RegionRiskBand::Low, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f, 1.1f);
    auto& med = CreateRegionRiskProfile(world, dbd::RegionRiskBand::Medium, 1.3f, 1.45f, 1.2f, 1.3f, 1.0f, 0.95f);
    auto& high = CreateRegionRiskProfile(world, dbd::RegionRiskBand::High, 1.7f, 2.25f, 1.6f, 1.7f, 1.2f, 0.8f);
    result.region_ids = {low.region_id, med.region_id, high.region_id};

    const dbd::UnitSkills laborer {3, 2, 3, 1, 1, 2};
    const dbd::UnitSkills runner {1, 1, 1, 3, 2, 3};
    const dbd::UnitSkills carrier {2, 2, 1, 4, 1, 2};
    const dbd::UnitSkills fighter {3, 1, 1, 1, 4, 2};

    auto& unit_a = CreateUnit(world, player.player_id, player.player_id, lineage.lineage_id, dbd::Vec3 {-3.0f, 0.0f, 0.0f}, squad.squad_id, laborer);
    auto& unit_b = CreateUnit(world, player.player_id, player.player_id, lineage.lineage_id, dbd::Vec3 {-1.0f, 0.0f, 1.5f}, squad.squad_id, runner);
    auto& unit_c = CreateUnit(world, player.player_id, player.player_id, lineage.lineage_id, dbd::Vec3 {1.0f, 0.0f, -1.0f}, squad.squad_id, carrier);
    auto& unit_d = CreateUnit(world, player.player_id, player.player_id, lineage.lineage_id, dbd::Vec3 {3.0f, 0.0f, 0.5f}, squad.squad_id, fighter);

    unit_a.combat_band = dbd::CombatRangeBand::Short;
    unit_b.combat_band = dbd::CombatRangeBand::Long;
    unit_c.combat_band = dbd::CombatRangeBand::Mid;
    unit_d.combat_band = dbd::CombatRangeBand::Short;

    const dbd::AutomationRule retreat_rule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.30f, true};
    const dbd::AutomationRule return_rule {dbd::AutomationTrigger::InventoryFull, dbd::AutomationAction::ReturnToStorage, 0.95f, true};
    const dbd::AutomationRule enemy_rule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::AttackNearestEnemy, 1.0f, true};
    unit_a.automation_rules = {retreat_rule};
    unit_b.automation_rules = {retreat_rule, enemy_rule};
    unit_c.automation_rules = {retreat_rule, return_rule};
    unit_d.automation_rules = {retreat_rule, enemy_rule};

    squad.stance = dbd::SquadStance::Defensive;
    squad.automation_rules = {retreat_rule};

    result.starter_unit_ids = {unit_a.unit_id, unit_b.unit_id, unit_c.unit_id, unit_d.unit_id};
    lineage.member_count = static_cast<std::uint32_t>(result.starter_unit_ids.size());

    auto& starter_node = CreateResourceNode(world, dbd::Vec3 {18.0f, 0.0f, 6.0f}, dbd::RegionRiskBand::Low, 1.0f, 1.0f, 1'000);
    auto& mid_node = CreateResourceNode(world, dbd::Vec3 {92.0f, 0.0f, 44.0f}, dbd::RegionRiskBand::Medium, 1.45f, 1.25f, 1'500);
    auto& frontier_node = CreateResourceNode(world, dbd::Vec3 {176.0f, 0.0f, 110.0f}, dbd::RegionRiskBand::High, 2.1f, 1.7f, 2'200);
    result.resource_node_ids = {starter_node.resource_node_id, mid_node.resource_node_id, frontier_node.resource_node_id};

    auto& depot = CreateStructure(
        world,
        player.player_id,
        dbd::StructureType::StorageDepot,
        dbd::BuildTimeClass::LargeSlow,
        dbd::Vec3 {6.0f, 0.0f, 0.0f},
        8.0f,
        0.14f);
    auto& storage = CreateStorageSite(world, depot.structure_id, player.player_id, depot.position);

    result.depot_structure_id = depot.structure_id;
    result.depot_storage_id = storage.storage_site_id;

    CreateInsurancePolicy(world, unit_a.unit_id, 150.0f, 0.15f, 0.55f);
    CreateInsurancePolicy(world, unit_c.unit_id, 250.0f, 0.25f, 0.72f);
    CreateInsurancePolicy(world, depot.structure_id, 800.0f, 0.30f, 0.70f);

    return result;
}

}  // namespace dbd_server
