#include "dbd_server/world_state.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace dbd_server {

namespace {

constexpr float kChunkSize = 64.0f;

float DistanceSquared(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float PseudoTerrainHeight(float x, float z) {
    return std::sin(x * 0.017f) * 8.0f + std::cos(z * 0.021f) * 6.0f + std::sin((x + z) * 0.011f) * 4.0f;
}

float ClampPositive(float value) {
    return value < 0.0f ? 0.0f : value;
}

std::string JoinCargo(const std::vector<dbd::CargoStack>& cargo) {
    if (cargo.empty()) {
        return "-";
    }
    std::ostringstream out;
    for (std::size_t i = 0; i < cargo.size(); ++i) {
        if (i != 0) {
            out << ';';
        }
        out << cargo[i].resource_id << ',' << cargo[i].amount << ',' << cargo[i].value_basis << ','
            << cargo[i].unit_weight << ',' << static_cast<int>(cargo[i].category);
    }
    return out.str();
}

std::vector<dbd::CargoStack> ParseCargo(const std::string& value) {
    std::vector<dbd::CargoStack> cargo;
    if (value.empty() || value == "-") {
        return cargo;
    }
    std::stringstream cargo_stream(value);
    std::string item;
    while (std::getline(cargo_stream, item, ';')) {
        if (item.empty()) {
            continue;
        }
        std::stringstream item_stream(item);
        std::string token;
        dbd::CargoStack stack;
        if (std::getline(item_stream, token, ',')) {
            stack.resource_id = static_cast<dbd::Id>(std::stoull(token));
        }
        if (std::getline(item_stream, token, ',')) {
            stack.amount = static_cast<std::uint32_t>(std::stoul(token));
        }
        if (std::getline(item_stream, token, ',')) {
            stack.value_basis = std::stof(token);
        }
        if (std::getline(item_stream, token, ',')) {
            stack.unit_weight = std::stof(token);
        }
        if (std::getline(item_stream, token, ',')) {
            stack.category = static_cast<dbd::CargoCategory>(std::stoi(token));
        }
        cargo.push_back(stack);
    }
    return cargo;
}

std::string JoinAutomationRules(const std::vector<dbd::AutomationRule>& rules) {
    if (rules.empty()) {
        return "-";
    }
    std::ostringstream out;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (i != 0) {
            out << ';';
        }
        out << static_cast<int>(rules[i].trigger) << ',' << static_cast<int>(rules[i].action) << ','
            << rules[i].threshold << ',' << rules[i].enabled;
    }
    return out.str();
}

std::vector<dbd::AutomationRule> ParseAutomationRules(const std::string& value) {
    std::vector<dbd::AutomationRule> rules;
    if (value.empty() || value == "-") {
        return rules;
    }
    std::stringstream rules_stream(value);
    std::string item;
    while (std::getline(rules_stream, item, ';')) {
        if (item.empty()) {
            continue;
        }

        std::stringstream item_stream(item);
        std::string token;
        dbd::AutomationRule rule;
        int trigger = 0;
        int action = 0;
        if (std::getline(item_stream, token, ',')) {
            trigger = std::stoi(token);
        }
        if (std::getline(item_stream, token, ',')) {
            action = std::stoi(token);
        }
        if (std::getline(item_stream, token, ',')) {
            rule.threshold = std::stof(token);
        }
        if (std::getline(item_stream, token, ',')) {
            rule.enabled = std::stoi(token) != 0;
        }

        rule.trigger = static_cast<dbd::AutomationTrigger>(trigger);
        rule.action = static_cast<dbd::AutomationAction>(action);
        rules.push_back(rule);
    }

    return rules;
}

std::filesystem::path ChunkFilePath(const std::filesystem::path& root_dir, dbd::ChunkCoord coord) {
    std::ostringstream name;
    name << "chunk_" << coord.x << "_" << coord.z << ".txt";
    return root_dir / name.str();
}

dbd::BuildTimeClass DefaultTimeClassFor(dbd::StructureType structure_type) {
    switch (structure_type) {
        case dbd::StructureType::DefenseNode:
            return dbd::BuildTimeClass::SmallFast;
        case dbd::StructureType::Extractor:
            return dbd::BuildTimeClass::MediumStandard;
        case dbd::StructureType::StorageDepot:
        default:
            return dbd::BuildTimeClass::LargeSlow;
    }
}

float DefaultHealthFor(dbd::StructureType structure_type) {
    switch (structure_type) {
        case dbd::StructureType::DefenseNode:
            return 500.0f;
        case dbd::StructureType::Extractor:
            return 700.0f;
        case dbd::StructureType::StorageDepot:
        default:
            return 950.0f;
    }
}

}  // namespace

dbd::Id AllocateId(WorldState& world) {
    return world.next_id++;
}

std::int64_t ChunkKey(dbd::ChunkCoord coord) {
    return (static_cast<std::int64_t>(coord.x) << 32) ^ static_cast<std::uint32_t>(coord.z);
}

dbd::ChunkCoord WorldToChunk(const dbd::Vec3& position) {
    return {
        static_cast<std::int32_t>(std::floor(position.x / kChunkSize)),
        static_cast<std::int32_t>(std::floor(position.z / kChunkSize))};
}

dbd::ChunkState& GetOrCreateChunk(WorldState& world, dbd::ChunkCoord coord) {
    const auto key = ChunkKey(coord);
    auto it = world.chunks.find(key);
    if (it != world.chunks.end()) {
        it->second.opened_today = true;
        it->second.loaded = true;
        it->second.last_opened_utc_ms = world.now_utc_ms;
        return it->second;
    }

    dbd::ChunkState chunk;
    chunk.coord = coord;
    chunk.opened_today = true;
    chunk.loaded = true;
    chunk.last_opened_utc_ms = world.now_utc_ms;
    return world.chunks.emplace(key, chunk).first->second;
}

dbd::PlayerState& CreatePlayer(WorldState& world, const std::string& display_name) {
    dbd::PlayerState player;
    player.player_id = AllocateId(world);
    player.display_name = display_name;
    player.credits = 1'000.0f;
    return world.players.emplace(player.player_id, std::move(player)).first->second;
}

dbd::LineageState& CreateLineage(WorldState& world, dbd::Id controlling_player_id, const std::string& display_name) {
    dbd::LineageState lineage;
    lineage.lineage_id = AllocateId(world);
    lineage.controlling_player_id = controlling_player_id;
    lineage.display_name = display_name;
    return world.lineages.emplace(lineage.lineage_id, std::move(lineage)).first->second;
}

dbd::CommandAuthorityState& CreateAuthority(
    WorldState& world,
    dbd::Id grantor_player_id,
    dbd::Id grantee_player_id,
    dbd::AuthorityScopeType scope_type,
    dbd::Id scope_target_id,
    std::uint32_t permission_mask) {
    dbd::CommandAuthorityState authority;
    authority.authority_id = AllocateId(world);
    authority.grantor_player_id = grantor_player_id;
    authority.grantee_player_id = grantee_player_id;
    authority.scope_type = scope_type;
    authority.scope_target_id = scope_target_id;
    authority.permission_mask = permission_mask;
    return world.authorities.emplace(authority.authority_id, std::move(authority)).first->second;
}

dbd::SquadState& CreateSquad(WorldState& world, dbd::Id owner_player_id, dbd::Id commander_player_id, const std::string& name) {
    dbd::SquadState squad;
    squad.squad_id = AllocateId(world);
    squad.owner_player_id = owner_player_id;
    squad.commander_player_id = commander_player_id;
    squad.name = name;
    world.players[owner_player_id].squad_ids.push_back(squad.squad_id);
    return world.squads.emplace(squad.squad_id, std::move(squad)).first->second;
}

dbd::UnitState& CreateUnit(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id controller_player_id,
    dbd::Id lineage_id,
    const dbd::Vec3& position,
    dbd::Id squad_id,
    const dbd::UnitSkills& skills) {
    dbd::UnitState unit;
    unit.unit_id = AllocateId(world);
    unit.owner_player_id = owner_player_id;
    unit.controller_player_id = controller_player_id;
    unit.squad_id = squad_id;
    unit.position = position;
    unit.move_target = position;
    unit.skills = skills;
    unit.lineage.lineage_id = lineage_id;
    unit.carry_capacity = 30.0f + (skills.strength_level * 7.0f) + (skills.haul_level * 5.0f);
    unit.upkeep_cost = 1.0f + (skills.combat_level * 0.12f) + (skills.construction_level * 0.06f);
    unit.tax_weight = 1.0f + (skills.haul_level * 0.08f);
    unit.food_demand = 1.0f + (skills.strength_level * 0.05f);
    unit.max_stamina = 100.0f;
    unit.stamina = unit.max_stamina;
    unit.overburden_threshold = unit.carry_capacity;
    unit.combat_band = dbd::CombatRangeBand::Mid;
    auto& created = world.units.emplace(unit.unit_id, std::move(unit)).first->second;
    world.players[owner_player_id].unit_ids.push_back(created.unit_id);
    world.squads[squad_id].unit_ids.push_back(created.unit_id);
    if (world.squads[squad_id].leader_unit_id == 0) {
        world.squads[squad_id].leader_unit_id = created.unit_id;
    }
    GetOrCreateChunk(world, WorldToChunk(position));
    return created;
}

dbd::ResourceNodeState& CreateResourceNode(
    WorldState& world,
    const dbd::Vec3& position,
    dbd::RegionRiskBand risk_band,
    float richness,
    float extraction_rate,
    std::uint32_t remaining_amount) {
    dbd::ResourceNodeState node;
    node.resource_node_id = AllocateId(world);
    node.position = position;
    node.chunk = WorldToChunk(position);
    node.risk_band = risk_band;
    node.richness = richness;
    node.extraction_rate = extraction_rate;
    node.remaining_amount = remaining_amount;
    node.max_amount = remaining_amount;
    GetOrCreateChunk(world, node.chunk);
    return world.resource_nodes.emplace(node.resource_node_id, std::move(node)).first->second;
}

dbd::StructureState& CreateStructure(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::StructureType structure_type,
    dbd::BuildTimeClass time_class,
    const dbd::Vec3& position,
    float footprint_radius,
    float allowed_slope) {
    dbd::StructureState structure;
    structure.structure_id = AllocateId(world);
    structure.owner_player_id = owner_player_id;
    structure.structure_type = structure_type;
    structure.time_class = time_class;
    structure.position = position;
    structure.chunk = WorldToChunk(position);
    structure.footprint_radius = footprint_radius;
    structure.allowed_slope = allowed_slope;
    structure.max_health = DefaultHealthFor(structure_type);
    structure.health = structure.max_health;
    world.players[owner_player_id].structure_ids.push_back(structure.structure_id);
    GetOrCreateChunk(world, structure.chunk);
    return world.structures.emplace(structure.structure_id, std::move(structure)).first->second;
}

dbd::StorageSiteState& CreateStorageSite(
    WorldState& world,
    dbd::Id structure_id,
    dbd::Id owner_player_id,
    const dbd::Vec3& position) {
    dbd::StorageSiteState storage;
    storage.storage_site_id = AllocateId(world);
    storage.structure_id = structure_id;
    storage.owner_player_id = owner_player_id;
    storage.position = position;
    storage.chunk = WorldToChunk(position);
    GetOrCreateChunk(world, storage.chunk);
    return world.storage_sites.emplace(storage.storage_site_id, std::move(storage)).first->second;
}

dbd::InsurancePolicyState& CreateInsurancePolicy(
    WorldState& world,
    dbd::Id insured_entity_id,
    float insured_value,
    float premium_rate,
    float payout_rate) {
    dbd::InsurancePolicyState policy;
    policy.policy_id = AllocateId(world);
    policy.insured_entity_id = insured_entity_id;
    policy.insured_value = insured_value;
    policy.premium_rate = premium_rate;
    policy.payout_rate = payout_rate;
    policy.active = true;
    return world.insurance_policies.emplace(policy.policy_id, std::move(policy)).first->second;
}

dbd::RegionRiskProfileState& CreateRegionRiskProfile(
    WorldState& world,
    dbd::RegionRiskBand band,
    float resource_density,
    float value_multiplier,
    float travel_difficulty,
    float flattening_cost_multiplier,
    float defense_score,
    float survival_score) {
    dbd::RegionRiskProfileState region;
    region.region_id = AllocateId(world);
    region.band = band;
    region.resource_density = resource_density;
    region.resource_value_multiplier = value_multiplier;
    region.travel_difficulty = travel_difficulty;
    region.flattening_cost_multiplier = flattening_cost_multiplier;
    region.defense_score = defense_score;
    region.survival_score = survival_score;
    return world.region_profiles.emplace(region.region_id, std::move(region)).first->second;
}

dbd::FlattenJobState& CreateFlattenJob(
    WorldState& world,
    const dbd::Vec3& center,
    float radius,
    float target_grade,
    float required_labor,
    float initial_slope) {
    dbd::FlattenJobState job;
    job.flatten_job_id = AllocateId(world);
    job.center = center;
    job.radius = radius;
    job.target_grade = target_grade;
    job.chunk = WorldToChunk(center);
    job.required_labor = required_labor;
    job.accumulated_labor = 0.0f;
    job.initial_slope = initial_slope;
    job.current_slope = initial_slope;
    job.state = dbd::FlattenJobStateKind::Planned;
    GetOrCreateChunk(world, job.chunk);
    return world.flatten_jobs.emplace(job.flatten_job_id, std::move(job)).first->second;
}

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
    dbd::Id flatten_job_id) {
    dbd::ConstructionSiteState site;
    site.construction_site_id = AllocateId(world);
    site.owner_player_id = owner_player_id;
    site.structure_type = structure_type;
    site.time_class = time_class;
    site.position = position;
    site.chunk = WorldToChunk(position);
    site.footprint_radius = footprint_radius;
    site.allowed_slope = allowed_slope;
    site.required_labor = required_labor;
    site.flattening_required = flattening_required;
    site.flatten_job_id = flatten_job_id;
    site.stage = flattening_required ? dbd::ConstructionStage::WaitingForFlatten : dbd::ConstructionStage::UnderConstruction;
    site.max_health = 180.0f + (footprint_radius * 18.0f);
    site.health = site.max_health;
    GetOrCreateChunk(world, site.chunk);
    return world.construction_sites.emplace(site.construction_site_id, std::move(site)).first->second;
}

dbd::DroppedCargoState& CreateDroppedCargo(
    WorldState& world,
    dbd::Id source_unit_id,
    const dbd::Vec3& position,
    const std::vector<dbd::CargoStack>& cargo) {
    dbd::DroppedCargoState drop;
    drop.dropped_cargo_id = AllocateId(world);
    drop.source_unit_id = source_unit_id;
    drop.position = position;
    drop.chunk = WorldToChunk(position);
    drop.cargo = cargo;
    drop.spawned_at_utc_ms = world.now_utc_ms;
    GetOrCreateChunk(world, drop.chunk);
    return world.dropped_cargo.emplace(drop.dropped_cargo_id, std::move(drop)).first->second;
}

dbd::CorpseState& CreateCorpse(WorldState& world, dbd::Id former_unit_id, const dbd::Vec3& position) {
    dbd::CorpseState corpse;
    corpse.corpse_id = AllocateId(world);
    corpse.former_unit_id = former_unit_id;
    corpse.position = position;
    corpse.chunk = WorldToChunk(position);
    corpse.time_of_death_utc_ms = world.now_utc_ms;
    GetOrCreateChunk(world, corpse.chunk);
    return world.corpses.emplace(corpse.corpse_id, std::move(corpse)).first->second;
}

std::optional<dbd::Id> FindNearestStorageSite(const WorldState& world, dbd::Id owner_player_id, const dbd::Vec3& from) {
    float best_distance = std::numeric_limits<float>::max();
    std::optional<dbd::Id> best_storage;

    for (const auto& [storage_id, storage] : world.storage_sites) {
        if (storage.owner_player_id != owner_player_id) {
            continue;
        }

        const float distance = DistanceSquared(storage.position, from);
        if (distance < best_distance) {
            best_distance = distance;
            best_storage = storage_id;
        }
    }

    return best_storage;
}

dbd::TerrainSample EvaluateTerrainAt(const WorldState& world, const dbd::Vec3& center, float radius, float allowed_slope) {
    const float sample_a = PseudoTerrainHeight(center.x - radius, center.z - radius);
    const float sample_b = PseudoTerrainHeight(center.x + radius, center.z);
    const float sample_c = PseudoTerrainHeight(center.x, center.z + radius);
    const float sample_d = PseudoTerrainHeight(center.x + radius, center.z + radius);
    const float min_height = std::min(std::min(sample_a, sample_b), std::min(sample_c, sample_d));
    const float max_height = std::max(std::max(sample_a, sample_b), std::max(sample_c, sample_d));
    float roughness = ClampPositive(max_height - min_height);
    float average_slope = roughness / std::max(6.0f, radius * 2.0f);

    for (const auto& [_, job] : world.flatten_jobs) {
        if (job.state != dbd::FlattenJobStateKind::Completed) {
            continue;
        }
        const float dx = job.center.x - center.x;
        const float dz = job.center.z - center.z;
        const float combined_radius = job.radius + radius;
        if ((dx * dx) + (dz * dz) > (combined_radius * combined_radius)) {
            continue;
        }

        average_slope = std::min(average_slope, job.target_grade);
        roughness *= 0.6f;
    }

    const auto chunk_it = world.chunks.find(ChunkKey(WorldToChunk(center)));
    if (chunk_it != world.chunks.end()) {
        for (const auto& stamp : chunk_it->second.flatten_stamps) {
            const float dx = stamp.center.x - center.x;
            const float dz = stamp.center.z - center.z;
            const float combined_radius = stamp.radius + radius;
            if ((dx * dx) + (dz * dz) > (combined_radius * combined_radius)) {
                continue;
            }
            average_slope = std::min(average_slope, stamp.applied_grade);
            roughness *= 0.55f;
        }
    }

    const bool buildable = average_slope <= allowed_slope;
    const float flatten_work = buildable ? 0.0f : (average_slope - allowed_slope) * radius * radius * 8.0f;

    dbd::TerrainSample sample;
    sample.average_slope = average_slope;
    sample.roughness = roughness;
    sample.flatten_work_estimate = flatten_work;
    sample.already_buildable = buildable;
    return sample;
}

void AdvanceWorldTime(WorldState& world, std::uint64_t delta_ms) {
    world.tick += delta_ms;
    world.now_utc_ms += delta_ms;
}

void PerformDailyChunkMaintenance(WorldState& world) {
    for (auto& [_, node] : world.resource_nodes) {
        const auto chunk_key = ChunkKey(node.chunk);
        const auto chunk_it = world.chunks.find(chunk_key);
        if (chunk_it != world.chunks.end() && chunk_it->second.opened_today) {
            node.remaining_amount = node.max_amount;
        }
    }

    for (auto it = world.corpses.begin(); it != world.corpses.end();) {
        const auto chunk_it = world.chunks.find(ChunkKey(it->second.chunk));
        if (chunk_it != world.chunks.end() && chunk_it->second.opened_today) {
            it = world.corpses.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = world.dropped_cargo.begin(); it != world.dropped_cargo.end();) {
        const auto chunk_it = world.chunks.find(ChunkKey(it->second.chunk));
        if (chunk_it != world.chunks.end() && chunk_it->second.opened_today) {
            it = world.dropped_cargo.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& [_, chunk] : world.chunks) {
        chunk.opened_today = false;
    }
}

bool SaveWorldState(const WorldState& world, const std::filesystem::path& root_dir) {
    std::error_code ec;
    std::filesystem::create_directories(root_dir, ec);
    if (ec) {
        return false;
    }

    for (const auto& [_, chunk] : world.chunks) {
        std::ofstream out(ChunkFilePath(root_dir, chunk.coord), std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out << "CHUNK " << chunk.coord.x << ' ' << chunk.coord.z << ' ' << chunk.last_opened_utc_ms << '\n';
        for (const auto& stamp : chunk.flatten_stamps) {
            out << "FLATTEN "
                << stamp.center.x << ' ' << stamp.center.y << ' ' << stamp.center.z << ' '
                << stamp.radius << ' ' << stamp.applied_grade << '\n';
        }

        for (const auto& [id, site] : world.construction_sites) {
            if (site.chunk.x != chunk.coord.x || site.chunk.z != chunk.coord.z) {
                continue;
            }
            out << "SITE "
                << id << ' ' << site.owner_player_id << ' ' << static_cast<int>(site.structure_type) << ' '
                << static_cast<int>(site.time_class) << ' '
                << site.position.x << ' ' << site.position.y << ' ' << site.position.z << ' '
                << site.footprint_radius << ' ' << site.allowed_slope << ' '
                << site.required_labor << ' ' << site.accumulated_labor << ' '
                << site.flatten_job_id << ' ' << site.flattening_required << ' '
                << static_cast<int>(site.stage) << ' ' << site.health << ' ' << site.max_health << '\n';
        }

        for (const auto& [id, structure] : world.structures) {
            if (structure.chunk.x != chunk.coord.x || structure.chunk.z != chunk.coord.z) {
                continue;
            }
            out << "STRUCT "
                << id << ' ' << structure.owner_player_id << ' ' << static_cast<int>(structure.structure_type) << ' '
                << static_cast<int>(structure.time_class) << ' '
                << structure.position.x << ' ' << structure.position.y << ' ' << structure.position.z << ' '
                << structure.footprint_radius << ' ' << structure.allowed_slope << ' '
                << structure.health << ' ' << structure.max_health << ' ' << structure.insurable << '\n';
        }

        for (const auto& [id, storage] : world.storage_sites) {
            if (storage.chunk.x != chunk.coord.x || storage.chunk.z != chunk.coord.z) {
                continue;
            }
            out << "STORAGE "
                << id << ' ' << storage.structure_id << ' ' << storage.owner_player_id << ' '
                << storage.position.x << ' ' << storage.position.y << ' ' << storage.position.z << ' '
                << JoinCargo(storage.stored_resources) << '\n';
        }

        for (const auto& [id, unit] : world.units) {
            if (WorldToChunk(unit.position).x != chunk.coord.x || WorldToChunk(unit.position).z != chunk.coord.z) {
                continue;
            }
            out << "UNIT "
                << id << ' ' << unit.owner_player_id << ' ' << unit.controller_player_id << ' ' << unit.squad_id << ' '
                << unit.lineage.lineage_id << ' '
                << unit.position.x << ' ' << unit.position.y << ' ' << unit.position.z << ' '
                << unit.move_target.x << ' ' << unit.move_target.y << ' ' << unit.move_target.z << ' '
                << unit.health << ' ' << unit.max_health << ' '
                << unit.alive << ' ' << unit.permanently_dead << ' '
                << static_cast<int>(unit.current_order) << ' '
                << static_cast<int>(unit.assignment.kind) << ' ' << unit.assignment.target_entity_id << ' '
                << static_cast<int>(unit.assignment.target_kind) << ' '
                << unit.assignment.target_position.x << ' ' << unit.assignment.target_position.y << ' ' << unit.assignment.target_position.z << ' '
                << unit.assignment.delegated << ' '
                << static_cast<int>(unit.combat_band) << ' '
                << unit.stamina << ' ' << unit.max_stamina << ' ' << unit.overburden_threshold << ' '
                << unit.carry_capacity << ' ' << unit.base_move_speed << ' '
                << unit.skills.strength_level << ' ' << unit.skills.harvest_level << ' '
                << unit.skills.construction_level << ' ' << unit.skills.haul_level << ' '
                << unit.skills.combat_level << ' ' << unit.skills.survival_level << ' '
                << unit.skill_progress.strength_xp << ' ' << unit.skill_progress.harvest_xp << ' '
                << unit.skill_progress.construction_xp << ' ' << unit.skill_progress.haul_xp << ' '
                << unit.skill_progress.combat_xp << ' ' << unit.skill_progress.survival_xp << ' '
                << JoinCargo(unit.cargo) << ' ' << JoinAutomationRules(unit.automation_rules) << '\n';
        }
    }

    std::ofstream meta(root_dir / "world_meta.txt", std::ios::trunc);
    if (!meta.is_open()) {
        return false;
    }
    meta << world.tick << '\n' << world.now_utc_ms << '\n' << world.next_id << '\n';
    for (const auto& [id, player] : world.players) {
        meta << "PLAYER "
             << id << ' ' << std::quoted(player.display_name) << ' ' << player.credits << ' '
             << player.tax_load << ' ' << player.upkeep_load << ' '
             << player.complexity_load << ' ' << player.food_load << ' '
             << player.food_shortage_ratio << ' ' << player.upkeep_shortage_ratio << '\n';
    }
    for (const auto& [id, lineage] : world.lineages) {
        meta << "LINEAGE "
             << id << ' ' << lineage.parent_lineage_id << ' ' << lineage.founder_unit_id << ' '
             << lineage.controlling_player_id << ' ' << std::quoted(lineage.display_name) << ' '
             << lineage.member_count << ' ' << lineage.military_value << ' ' << lineage.economic_value << '\n';
    }
    for (const auto& [id, squad] : world.squads) {
        meta << "SQUAD "
             << id << ' ' << squad.owner_player_id << ' ' << squad.commander_player_id << ' '
             << squad.leader_unit_id << ' ' << static_cast<int>(squad.stance) << ' '
             << std::quoted(squad.name) << ' ' << JoinAutomationRules(squad.automation_rules) << '\n';
    }
    for (const auto& [id, auth] : world.authorities) {
        meta << "AUTH "
             << id << ' ' << auth.grantor_player_id << ' ' << auth.grantee_player_id << ' '
             << static_cast<int>(auth.scope_type) << ' ' << auth.scope_target_id << ' '
             << auth.permission_mask << ' ' << auth.revocable << ' ' << auth.expires_at_utc_ms << '\n';
    }
    for (const auto& [id, policy] : world.insurance_policies) {
        meta << "POLICY "
             << id << ' ' << policy.insured_entity_id << ' ' << policy.insured_value << ' '
             << policy.premium_rate << ' ' << policy.payout_rate << ' '
             << policy.activation_time_utc_ms << ' ' << policy.active << '\n';
    }
    return true;
}

bool LoadWorldState(WorldState& world, const std::filesystem::path& root_dir) {
    world = WorldState {};
    if (!std::filesystem::exists(root_dir)) {
        return false;
    }

    const auto meta_path = root_dir / "world_meta.txt";
    if (std::filesystem::exists(meta_path)) {
        std::ifstream meta(meta_path);
        if (!meta.is_open()) {
            return false;
        }
        meta >> world.tick;
        meta >> world.now_utc_ms;
        meta >> world.next_id;
        std::string kind;
        while (meta >> kind) {
            if (kind == "PLAYER") {
                dbd::PlayerState player;
                meta >> player.player_id >> std::quoted(player.display_name) >> player.credits
                    >> player.tax_load >> player.upkeep_load >> player.complexity_load >> player.food_load
                    >> player.food_shortage_ratio >> player.upkeep_shortage_ratio;
                world.players.emplace(player.player_id, player);
            } else if (kind == "LINEAGE") {
                dbd::LineageState lineage;
                meta >> lineage.lineage_id >> lineage.parent_lineage_id >> lineage.founder_unit_id
                    >> lineage.controlling_player_id >> std::quoted(lineage.display_name)
                    >> lineage.member_count >> lineage.military_value >> lineage.economic_value;
                world.lineages.emplace(lineage.lineage_id, lineage);
            } else if (kind == "SQUAD") {
                dbd::SquadState squad;
                int stance = 0;
                std::string rules_text;
                meta >> squad.squad_id >> squad.owner_player_id >> squad.commander_player_id >> squad.leader_unit_id >> stance >> std::quoted(squad.name) >> rules_text;
                squad.stance = static_cast<dbd::SquadStance>(stance);
                squad.automation_rules = ParseAutomationRules(rules_text);
                world.squads.emplace(squad.squad_id, squad);
            } else if (kind == "AUTH") {
                dbd::CommandAuthorityState auth;
                int scope_type = 0;
                meta >> auth.authority_id >> auth.grantor_player_id >> auth.grantee_player_id
                    >> scope_type >> auth.scope_target_id >> auth.permission_mask >> auth.revocable >> auth.expires_at_utc_ms;
                auth.scope_type = static_cast<dbd::AuthorityScopeType>(scope_type);
                world.authorities.emplace(auth.authority_id, auth);
            } else if (kind == "POLICY") {
                dbd::InsurancePolicyState policy;
                meta >> policy.policy_id >> policy.insured_entity_id >> policy.insured_value
                    >> policy.premium_rate >> policy.payout_rate >> policy.activation_time_utc_ms >> policy.active;
                world.insurance_policies.emplace(policy.policy_id, policy);
            }
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(root_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == "world_meta.txt") {
            continue;
        }

        std::ifstream in(entry.path());
        if (!in.is_open()) {
            return false;
        }

        std::string kind;
        dbd::ChunkCoord chunk_coord {};
        while (in >> kind) {
            if (kind == "CHUNK") {
                dbd::ChunkState chunk;
                in >> chunk.coord.x >> chunk.coord.z >> chunk.last_opened_utc_ms;
                chunk_coord = chunk.coord;
                world.chunks.emplace(ChunkKey(chunk.coord), chunk);
            } else if (kind == "FLATTEN") {
                dbd::TerrainFlattenStamp stamp;
                in >> stamp.center.x >> stamp.center.y >> stamp.center.z >> stamp.radius >> stamp.applied_grade;
                GetOrCreateChunk(world, chunk_coord).flatten_stamps.push_back(stamp);
            } else if (kind == "SITE") {
                dbd::ConstructionSiteState site;
                int structure_type = 0;
                int time_class = 0;
                int stage = 0;
                in >> site.construction_site_id >> site.owner_player_id >> structure_type >> time_class
                    >> site.position.x >> site.position.y >> site.position.z
                    >> site.footprint_radius >> site.allowed_slope
                    >> site.required_labor >> site.accumulated_labor
                    >> site.flatten_job_id >> site.flattening_required
                    >> stage >> site.health >> site.max_health;
                site.structure_type = static_cast<dbd::StructureType>(structure_type);
                site.time_class = static_cast<dbd::BuildTimeClass>(time_class);
                site.stage = static_cast<dbd::ConstructionStage>(stage);
                site.chunk = chunk_coord;
                world.construction_sites.emplace(site.construction_site_id, site);
            } else if (kind == "STRUCT") {
                dbd::StructureState structure;
                int structure_type = 0;
                int time_class = 0;
                in >> structure.structure_id >> structure.owner_player_id >> structure_type >> time_class
                    >> structure.position.x >> structure.position.y >> structure.position.z
                    >> structure.footprint_radius >> structure.allowed_slope
                    >> structure.health >> structure.max_health >> structure.insurable;
                structure.structure_type = static_cast<dbd::StructureType>(structure_type);
                structure.time_class = static_cast<dbd::BuildTimeClass>(time_class);
                structure.chunk = chunk_coord;
                world.structures.emplace(structure.structure_id, structure);
                world.players[structure.owner_player_id].structure_ids.push_back(structure.structure_id);
            } else if (kind == "STORAGE") {
                dbd::StorageSiteState storage;
                std::string cargo_text;
                in >> storage.storage_site_id >> storage.structure_id >> storage.owner_player_id
                    >> storage.position.x >> storage.position.y >> storage.position.z;
                std::getline(in >> std::ws, cargo_text);
                storage.chunk = chunk_coord;
                storage.stored_resources = ParseCargo(cargo_text);
                world.storage_sites.emplace(storage.storage_site_id, storage);
            } else if (kind == "UNIT") {
                dbd::UnitState unit;
                int current_order = 0;
                int assignment_kind = 0;
                int target_kind = 0;
                int combat_band = 0;
                std::string cargo_text;
                std::string rules_text;
                in >> unit.unit_id >> unit.owner_player_id >> unit.controller_player_id >> unit.squad_id
                    >> unit.lineage.lineage_id
                    >> unit.position.x >> unit.position.y >> unit.position.z
                    >> unit.move_target.x >> unit.move_target.y >> unit.move_target.z
                    >> unit.health >> unit.max_health
                    >> unit.alive >> unit.permanently_dead
                    >> current_order
                    >> assignment_kind >> unit.assignment.target_entity_id >> target_kind
                    >> unit.assignment.target_position.x >> unit.assignment.target_position.y >> unit.assignment.target_position.z
                    >> unit.assignment.delegated
                    >> combat_band >> unit.stamina >> unit.max_stamina >> unit.overburden_threshold
                    >> unit.carry_capacity >> unit.base_move_speed
                    >> unit.skills.strength_level >> unit.skills.harvest_level
                    >> unit.skills.construction_level >> unit.skills.haul_level
                    >> unit.skills.combat_level >> unit.skills.survival_level
                    >> unit.skill_progress.strength_xp >> unit.skill_progress.harvest_xp
                    >> unit.skill_progress.construction_xp >> unit.skill_progress.haul_xp
                    >> unit.skill_progress.combat_xp >> unit.skill_progress.survival_xp
                    >> cargo_text >> rules_text;
                unit.current_order = static_cast<dbd::UnitOrderType>(current_order);
                unit.assignment.kind = static_cast<dbd::JobKind>(assignment_kind);
                unit.assignment.target_kind = static_cast<dbd::AttackTargetKind>(target_kind);
                unit.combat_band = static_cast<dbd::CombatRangeBand>(combat_band);
                unit.cargo = ParseCargo(cargo_text);
                unit.automation_rules = ParseAutomationRules(rules_text);
                if (unit.carry_capacity <= 0.0f) {
                    unit.carry_capacity = 30.0f + (unit.skills.strength_level * 7.0f) + (unit.skills.haul_level * 5.0f);
                }
                if (unit.base_move_speed <= 0.0f) {
                    unit.base_move_speed = 4.0f;
                }
                unit.upkeep_cost = 1.0f + (unit.skills.combat_level * 0.12f) + (unit.skills.construction_level * 0.06f);
                unit.tax_weight = 1.0f + (unit.skills.haul_level * 0.08f);
                unit.food_demand = 1.0f + (unit.skills.strength_level * 0.05f);
                world.units.emplace(unit.unit_id, unit);
                world.players[unit.owner_player_id].unit_ids.push_back(unit.unit_id);
                if (unit.squad_id != 0) {
                    world.squads[unit.squad_id].unit_ids.push_back(unit.unit_id);
                }
            }
        }
    }

    for (const auto& [_, unit] : world.units) {
        if (world.players.find(unit.owner_player_id) == world.players.end()) {
            return false;
        }
        if (unit.squad_id != 0 && world.squads.find(unit.squad_id) == world.squads.end()) {
            return false;
        }
        if (world.lineages.find(unit.lineage.lineage_id) == world.lineages.end()) {
            return false;
        }
    }

    for (const auto& [_, structure] : world.structures) {
        if (world.players.find(structure.owner_player_id) == world.players.end()) {
            return false;
        }
    }

    for (const auto& [_, storage] : world.storage_sites) {
        if (world.structures.find(storage.structure_id) == world.structures.end()) {
            return false;
        }
        if (world.players.find(storage.owner_player_id) == world.players.end()) {
            return false;
        }
    }

    for (const auto& [_, authority] : world.authorities) {
        if (world.players.find(authority.grantor_player_id) == world.players.end() ||
            world.players.find(authority.grantee_player_id) == world.players.end()) {
            return false;
        }
    }

    for (const auto& [_, policy] : world.insurance_policies) {
        const bool has_unit = world.units.find(policy.insured_entity_id) != world.units.end();
        const bool has_structure = world.structures.find(policy.insured_entity_id) != world.structures.end();
        if (!has_unit && !has_structure) {
            return false;
        }
    }

    return true;
}

}  // namespace dbd_server
