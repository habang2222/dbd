#pragma once

#include "dbd/domain_types.hpp"

#include <optional>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dbd_server {

struct WorldState {
    std::uint64_t tick {0};
    std::uint64_t now_utc_ms {0};
    dbd::Id next_id {1};

    std::unordered_map<dbd::Id, dbd::PlayerState> players;
    std::unordered_map<dbd::Id, dbd::CommandAuthorityState> authorities;
    std::unordered_map<dbd::Id, dbd::LineageState> lineages;
    std::unordered_map<dbd::Id, dbd::UnitState> units;
    std::unordered_map<dbd::Id, dbd::SquadState> squads;
    std::unordered_map<dbd::Id, dbd::ResourceNodeState> resource_nodes;
    std::unordered_map<dbd::Id, dbd::FlattenJobState> flatten_jobs;
    std::unordered_map<dbd::Id, dbd::ConstructionSiteState> construction_sites;
    std::unordered_map<dbd::Id, dbd::StructureState> structures;
    std::unordered_map<dbd::Id, dbd::StorageSiteState> storage_sites;
    std::unordered_map<dbd::Id, dbd::DroppedCargoState> dropped_cargo;
    std::unordered_map<dbd::Id, dbd::CorpseState> corpses;
    std::unordered_map<dbd::Id, dbd::InsurancePolicyState> insurance_policies;
    std::unordered_map<dbd::Id, dbd::RegionRiskProfileState> region_profiles;
    std::unordered_map<std::int64_t, dbd::ChunkState> chunks;
};

dbd::Id AllocateId(WorldState& world);
std::int64_t ChunkKey(dbd::ChunkCoord coord);
dbd::ChunkCoord WorldToChunk(const dbd::Vec3& position);
dbd::ChunkState& GetOrCreateChunk(WorldState& world, dbd::ChunkCoord coord);

dbd::PlayerState& CreatePlayer(WorldState& world, const std::string& display_name);
dbd::LineageState& CreateLineage(WorldState& world, dbd::Id controlling_player_id, const std::string& display_name);
dbd::CommandAuthorityState& CreateAuthority(
    WorldState& world,
    dbd::Id grantor_player_id,
    dbd::Id grantee_player_id,
    dbd::AuthorityScopeType scope_type,
    dbd::Id scope_target_id,
    std::uint32_t permission_mask);
dbd::UnitState& CreateUnit(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id controller_player_id,
    dbd::Id lineage_id,
    const dbd::Vec3& position,
    dbd::Id squad_id,
    const dbd::UnitSkills& skills);
dbd::SquadState& CreateSquad(WorldState& world, dbd::Id owner_player_id, dbd::Id commander_player_id, const std::string& name);
dbd::ResourceNodeState& CreateResourceNode(
    WorldState& world,
    const dbd::Vec3& position,
    dbd::RegionRiskBand risk_band,
    float richness,
    float extraction_rate,
    std::uint32_t remaining_amount);
dbd::StructureState& CreateStructure(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::StructureType structure_type,
    dbd::BuildTimeClass time_class,
    const dbd::Vec3& position,
    float footprint_radius,
    float allowed_slope);
dbd::StorageSiteState& CreateStorageSite(
    WorldState& world,
    dbd::Id structure_id,
    dbd::Id owner_player_id,
    const dbd::Vec3& position);
dbd::InsurancePolicyState& CreateInsurancePolicy(
    WorldState& world,
    dbd::Id insured_entity_id,
    float insured_value,
    float premium_rate,
    float payout_rate);
dbd::RegionRiskProfileState& CreateRegionRiskProfile(
    WorldState& world,
    dbd::RegionRiskBand band,
    float resource_density,
    float value_multiplier,
    float travel_difficulty,
    float flattening_cost_multiplier,
    float defense_score,
    float survival_score);
dbd::FlattenJobState& CreateFlattenJob(
    WorldState& world,
    const dbd::Vec3& center,
    float radius,
    float target_grade,
    float required_labor,
    float initial_slope);
dbd::ConstructionSiteState& CreateConstructionSite(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::StructureType structure_type,
    dbd::BuildTimeClass time_class,
    const dbd::Vec3& position,
    float footprint_radius,
    float allowed_slope,
    float required_labor,
    bool flattening_required,
    dbd::Id flatten_job_id);
dbd::DroppedCargoState& CreateDroppedCargo(
    WorldState& world,
    dbd::Id source_unit_id,
    const dbd::Vec3& position,
    const std::vector<dbd::CargoStack>& cargo);
dbd::CorpseState& CreateCorpse(WorldState& world, dbd::Id former_unit_id, const dbd::Vec3& position);

std::optional<dbd::Id> FindNearestStorageSite(const WorldState& world, dbd::Id owner_player_id, const dbd::Vec3& from);
dbd::TerrainSample EvaluateTerrainAt(const WorldState& world, const dbd::Vec3& center, float radius, float allowed_slope);
void AdvanceWorldTime(WorldState& world, std::uint64_t delta_ms);
void PerformDailyChunkMaintenance(WorldState& world);
bool SaveWorldState(const WorldState& world, const std::filesystem::path& root_dir);
bool LoadWorldState(WorldState& world, const std::filesystem::path& root_dir);

}  // namespace dbd_server
