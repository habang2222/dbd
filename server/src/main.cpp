#include "dbd_server/server_simulation.hpp"
#include "dbd_server/world_bootstrap.hpp"

#include <filesystem>
#include <iostream>

int main() {
    std::cout << "DBD authoritative server scaffold booting..." << std::endl;
    dbd_server::WorldState world;
    const auto bootstrap = dbd_server::BootstrapSinglePlayerWarSlice(world, "FoundingPlayer");
    const auto raider_bootstrap = dbd_server::BootstrapSinglePlayerWarSlice(world, "RaiderPlayer");

    std::cout << "Spawned player " << bootstrap.player_id
              << " with " << bootstrap.starter_unit_ids.size()
              << " flexible starter units." << std::endl;
    std::cout << "Spawned second player " << raider_bootstrap.player_id
              << " for contested-site testing." << std::endl;

    const auto stance_result = dbd_server::SetSquadStance(
        world,
        bootstrap.player_id,
        bootstrap.squad_id,
        dbd::SquadStance::Defensive);
    std::cout << "Squad stance test: " << stance_result.message << std::endl;

    const auto authority_result = dbd_server::GrantAuthority(
        world,
        bootstrap.player_id,
        raider_bootstrap.player_id,
        dbd::AuthorityScopeType::Unit,
        {bootstrap.starter_unit_ids[1]},
        static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueMove) |
            static_cast<std::uint32_t>(dbd::AuthorityPermission::IssueAttack),
        true,
        world.now_utc_ms + 60'000);
    std::cout << "Authority grant test: " << authority_result.message << std::endl;

    if (world.storage_sites.find(bootstrap.depot_storage_id) != world.storage_sites.end()) {
        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.push_back(
            dbd::CargoStack {80'001, 40, 200.0f, 1.0f, dbd::CargoCategory::Resource});
    }

    if (!bootstrap.starter_unit_ids.empty() && !bootstrap.resource_node_ids.empty()) {
        const auto harvest_result = dbd_server::IssueHarvestOrder(
            world,
            bootstrap.player_id,
            bootstrap.starter_unit_ids.front(),
            bootstrap.resource_node_ids.front());
        std::cout << "Harvest test: " << harvest_result.message << std::endl;

        auto& hauler = world.units.at(bootstrap.starter_unit_ids.front());
        hauler.cargo.push_back(dbd::CargoStack {90'001, 20, 140.0f, 3.0f, dbd::CargoCategory::Resource});
        std::cout << "Overburden test: cargo weight pushed near/over capacity for unit " << hauler.unit_id << std::endl;

        const auto automation_result = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Automation tick: " << automation_result.message << std::endl;
        for (int i = 0; i < 6; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            std::cout << "Movement tick " << (i + 1) << ": " << movement_result.message << std::endl;
        }
    }

    if (!raider_bootstrap.starter_unit_ids.empty() && !bootstrap.starter_unit_ids.empty()) {
        auto& raider = world.units.at(raider_bootstrap.starter_unit_ids.front());
        auto& defender = world.units.at(bootstrap.starter_unit_ids[1]);
        raider.position = {defender.position.x + 4.0f, defender.position.y, defender.position.z + 2.0f};
        const auto enemy_auto = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Enemy detection automation: " << enemy_auto.message << std::endl;
        for (int i = 0; i < 5; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            const auto combat_result = dbd_server::ProgressCombat(world, 1.0f);
            std::cout << "Combat pursuit tick " << (i + 1) << ": " << movement_result.message
                      << " | " << combat_result.message << std::endl;
        }
    }

    if (bootstrap.starter_unit_ids.size() > 2) {
        auto& wounded = world.units.at(bootstrap.starter_unit_ids[2]);
        wounded.health = 18.0f;
        const auto retreat_auto = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Low-health retreat automation: " << retreat_auto.message << std::endl;
        for (int i = 0; i < 4; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            std::cout << "Retreat movement tick " << (i + 1) << ": " << movement_result.message << std::endl;
        }
    }

    const auto flatten_result = dbd_server::StartFlattenJob(world, bootstrap.player_id, dbd::Vec3 {72.0f, 0.0f, 38.0f}, 8.0f, 0.14f);
    std::cout << "Flatten start: " << flatten_result.message << std::endl;
    if (flatten_result.ok) {
        dbd_server::AssignUnitsToFlattenJob(
            world,
            bootstrap.player_id,
            flatten_result.entity_id,
            {bootstrap.starter_unit_ids[0], bootstrap.starter_unit_ids[1]});

        for (int i = 0; i < 15; ++i) {
            dbd_server::ProgressFlattenJobs(world, 1.0f);
        }
    }

    const auto construction_result = dbd_server::StartConstruction(
        world,
        bootstrap.player_id,
        dbd::StructureType::Extractor,
        dbd::Vec3 {72.0f, 0.0f, 38.0f},
        8.0f);
    std::cout << "Construction start: " << construction_result.message << std::endl;

    if (construction_result.ok) {
        dbd_server::AssignUnitsToConstruction(
            world,
            bootstrap.player_id,
            construction_result.entity_id,
            {bootstrap.starter_unit_ids[0], bootstrap.starter_unit_ids[2], bootstrap.starter_unit_ids[3]});

        for (int i = 0; i < 8; ++i) {
            dbd_server::ProgressConstructionSites(world, 1.0f);
        }

        dbd_server::RemoveUnitsFromConstruction(
            world,
            bootstrap.player_id,
            construction_result.entity_id,
            {bootstrap.starter_unit_ids[3]});
        std::cout << "Partial reassignment test: pulled one escort unit off construction." << std::endl;

        const auto raid_result = dbd_server::ResolveAttack(
            world,
            raider_bootstrap.player_id,
            {raider_bootstrap.starter_unit_ids[0], raider_bootstrap.starter_unit_ids[1]},
            dbd::AttackTargetKind::ConstructionSite,
            construction_result.entity_id);
        std::cout << "Raid test: " << raid_result.message << std::endl;
        for (int i = 0; i < 6; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            const auto combat_result = dbd_server::ProgressCombat(world, 1.0f);
            std::cout << "Worksite pressure tick " << (i + 1) << ": " << movement_result.message
                      << " | " << combat_result.message << std::endl;
        }

        dbd_server::AssignUnitsToConstruction(
            world,
            bootstrap.player_id,
            construction_result.entity_id,
            {bootstrap.starter_unit_ids[1]});

        for (int i = 0; i < 24; ++i) {
            dbd_server::ProgressConstructionSites(world, 1.0f);
        }
    }

    const auto economic_result = dbd_server::ProcessEconomicSettlement(world, 10.0f);
    std::cout << "Economic settlement: " << economic_result.message << std::endl;

    const auto death_result = dbd_server::ResolvePermanentUnitDeath(world, bootstrap.starter_unit_ids.back());
    std::cout << "Permanent death test: " << death_result.message << std::endl;

    dbd_server::PerformDailyChunkMaintenance(world);
    std::cout << "Maintenance test complete. Persistent world state kept; resources/drops/corpses in opened chunks reset." << std::endl;

    const auto save_root = std::filesystem::path("C:\\Users\\victo\\OneDrive\\Desktop\\dbd\\DBDReboot\\runtime-save");
    const bool save_ok = dbd_server::SaveWorldState(world, save_root);
    std::cout << "Save test: " << (save_ok ? "world saved." : "save failed.") << std::endl;

    dbd_server::WorldState loaded_world;
    const bool load_ok = dbd_server::LoadWorldState(loaded_world, save_root);
    std::cout << "Load test: " << (load_ok ? "world loaded." : "load failed.") << std::endl;
    return 0;
}
