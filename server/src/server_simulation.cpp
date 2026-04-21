#include "dbd_server/server_simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace dbd_server {

namespace {

template <typename T>
bool ContainsId(const std::vector<T>& values, T needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

template <typename T>
void EraseIds(std::vector<T>& values, const std::vector<T>& removals) {
    values.erase(
        std::remove_if(values.begin(), values.end(), [&](T value) { return ContainsId(removals, value); }),
        values.end());
}

bool HasPermission(const dbd::CommandAuthorityState& authority, dbd::AuthorityPermission permission) {
    const auto mask = static_cast<std::uint32_t>(permission);
    return (authority.permission_mask & mask) == mask;
}

bool HasControl(const WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::AuthorityPermission permission) {
    const auto it = world.units.find(unit_id);
    if (it == world.units.end() || !it->second.alive || it->second.permanently_dead) {
        return false;
    }

    if (it->second.controller_player_id == controller_player_id) {
        return true;
    }

    for (const auto& [_, authority] : world.authorities) {
        if (authority.grantee_player_id != controller_player_id) {
            continue;
        }
        if (authority.expires_at_utc_ms != 0 && authority.expires_at_utc_ms < world.now_utc_ms) {
            continue;
        }
        if (!HasPermission(authority, permission)) {
            continue;
        }
        if (authority.scope_type == dbd::AuthorityScopeType::Unit && authority.scope_target_id == unit_id) {
            return true;
        }
        if (authority.scope_type == dbd::AuthorityScopeType::Squad && authority.scope_target_id == it->second.squad_id) {
            return true;
        }
        if (authority.scope_type == dbd::AuthorityScopeType::LineageWide &&
            authority.scope_target_id == it->second.lineage.lineage_id) {
            return true;
        }
    }

    return false;
}

dbd::BuildTimeClass BuildTimeClassFor(dbd::StructureType structure_type, float footprint_radius) {
    if (footprint_radius <= 6.0f || structure_type == dbd::StructureType::DefenseNode) {
        return dbd::BuildTimeClass::SmallFast;
    }
    if (footprint_radius <= 10.0f || structure_type == dbd::StructureType::Extractor) {
        return dbd::BuildTimeClass::MediumStandard;
    }
    return dbd::BuildTimeClass::LargeSlow;
}

float BaseLaborForBuildClass(dbd::BuildTimeClass time_class) {
    switch (time_class) {
        case dbd::BuildTimeClass::SmallFast:
            return 45.0f;
        case dbd::BuildTimeClass::MediumStandard:
            return 120.0f;
        case dbd::BuildTimeClass::LargeSlow:
        default:
            return 260.0f;
    }
}

float AllowedSlopeForStructure(dbd::StructureType structure_type) {
    switch (structure_type) {
        case dbd::StructureType::DefenseNode:
            return 0.20f;
        case dbd::StructureType::Extractor:
            return 0.14f;
        case dbd::StructureType::StorageDepot:
        default:
            return 0.12f;
    }
}

float ConstructionLaborPerSecond(const dbd::UnitState& unit) {
    return 1.0f + (unit.skills.strength_level * 0.55f) + (unit.skills.construction_level * 0.9f);
}

float HarvestLaborPerSecond(const dbd::UnitState& unit) {
    return 1.0f + (unit.skills.harvest_level * 0.85f) + (unit.skills.strength_level * 0.25f);
}

float BaseCombatDamagePerSecond(const dbd::UnitState& unit) {
    float base = 3.0f + (unit.skills.combat_level * 1.4f) + (unit.skills.strength_level * 0.4f);
    switch (unit.combat_band) {
        case dbd::CombatRangeBand::Short:
            return base * 1.2f;
        case dbd::CombatRangeBand::Long:
            return base * 0.9f;
        case dbd::CombatRangeBand::Mid:
        default:
            return base;
    }
}

float DistanceSquared(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float Distance(const dbd::Vec3& a, const dbd::Vec3& b) {
    return std::sqrt(DistanceSquared(a, b));
}

float MaxAttackRangeFor(const dbd::UnitState& unit) {
    const float skill_bonus = unit.skills.combat_level * 0.75f;
    switch (unit.combat_band) {
        case dbd::CombatRangeBand::Short:
            return 6.0f + skill_bonus;
        case dbd::CombatRangeBand::Long:
            return 15.0f + skill_bonus;
        case dbd::CombatRangeBand::Mid:
        default:
            return 10.0f + skill_bonus;
    }
}

float CargoValue(const std::vector<dbd::CargoStack>& cargo) {
    float total = 0.0f;
    for (const auto& stack : cargo) {
        total += stack.value_basis;
    }
    return total;
}

float CargoWeight(const std::vector<dbd::CargoStack>& cargo) {
    float total = 0.0f;
    for (const auto& stack : cargo) {
        total += static_cast<float>(stack.amount) * stack.unit_weight;
    }
    return total;
}

float CargoFillRatio(const dbd::UnitState& unit) {
    return CargoWeight(unit.cargo) / std::max(1.0f, unit.carry_capacity);
}

bool IsOverburdened(const dbd::UnitState& unit) {
    return CargoWeight(unit.cargo) > unit.overburden_threshold;
}

void ApplyMovementStaminaCost(dbd::UnitState& unit, float delta_seconds) {
    const bool running = unit.current_order == dbd::UnitOrderType::Move || unit.current_order == dbd::UnitOrderType::Retreat;
    if (!running) {
        unit.stamina = std::min(unit.max_stamina, unit.stamina + (4.0f * delta_seconds));
        return;
    }

    float drain = 6.0f * delta_seconds;
    if (unit.current_order == dbd::UnitOrderType::Retreat) {
        drain += 3.0f * delta_seconds;
    }
    if (IsOverburdened(unit)) {
        drain += 5.0f * delta_seconds;
    }

    unit.stamina = std::max(0.0f, unit.stamina - drain);
}

float EffectiveMoveSpeed(const dbd::UnitState& unit) {
    float speed = unit.base_move_speed;
    switch (unit.current_order) {
        case dbd::UnitOrderType::Scout:
            speed *= 0.7f;
            break;
        case dbd::UnitOrderType::AttackTarget:
            speed *= 1.05f;
            break;
        case dbd::UnitOrderType::Retreat:
            speed *= unit.stamina > 20.0f ? 1.5f : 1.05f;
            break;
        case dbd::UnitOrderType::HaulToStorage:
            speed *= 0.95f;
            break;
        case dbd::UnitOrderType::Move:
        default:
            break;
    }

    const float cargo_ratio = CargoFillRatio(unit);
    if (cargo_ratio > 1.0f) {
        speed *= std::max(0.35f, 1.0f - ((cargo_ratio - 1.0f) * 0.45f));
    }
    if (IsOverburdened(unit)) {
        speed *= 0.8f;
    }
    if (unit.current_order == dbd::UnitOrderType::Retreat && unit.stamina <= 5.0f) {
        speed *= 0.7f;
    }

    return std::max(0.5f, speed);
}

std::optional<dbd::Id> FindNearestEnemyUnit(const WorldState& world, const dbd::UnitState& unit) {
    float best_distance = std::numeric_limits<float>::max();
    std::optional<dbd::Id> best_enemy;
    const float vision_range = 14.0f + (unit.skills.survival_level * 0.75f);
    const float vision_range_sq = vision_range * vision_range;

    for (const auto& [other_id, other] : world.units) {
        if (!other.alive || other.permanently_dead || other_id == unit.unit_id) {
            continue;
        }
        if (other.owner_player_id == unit.owner_player_id) {
            continue;
        }
        const float distance_sq = DistanceSquared(unit.position, other.position);
        if (distance_sq > vision_range_sq) {
            continue;
        }
        if (distance_sq < best_distance) {
            best_distance = distance_sq;
            best_enemy = other_id;
        }
    }

    return best_enemy;
}

dbd::Vec3 ComputeRetreatTarget(const WorldState& world, const dbd::UnitState& unit) {
    const auto nearest_storage = FindNearestStorageSite(world, unit.owner_player_id, unit.position);
    if (nearest_storage.has_value()) {
        const auto storage_it = world.storage_sites.find(*nearest_storage);
        if (storage_it != world.storage_sites.end()) {
            return storage_it->second.position;
        }
    }

    return {unit.position.x - 12.0f, unit.position.y, unit.position.z - 12.0f};
}

bool EnemySeenNearby(const WorldState& world, const dbd::UnitState& unit) {
    const float vision_range = 14.0f + (unit.skills.survival_level * 0.75f);
    const float vision_range_sq = vision_range * vision_range;
    for (const auto& [_, other] : world.units) {
        if (!other.alive || other.permanently_dead || other.unit_id == unit.unit_id) {
            continue;
        }
        if (other.owner_player_id == unit.owner_player_id) {
            continue;
        }
        if (DistanceSquared(unit.position, other.position) <= vision_range_sq) {
            return true;
        }
    }
    return false;
}

void ClearUnitAssignment(dbd::UnitState& unit);

void RemoveUnitFromAssignedJobs(WorldState& world, dbd::Id unit_id) {
    for (auto& [_, job] : world.flatten_jobs) {
        job.assigned_unit_ids.erase(
            std::remove(job.assigned_unit_ids.begin(), job.assigned_unit_ids.end(), unit_id),
            job.assigned_unit_ids.end());
    }

    for (auto& [_, site] : world.construction_sites) {
        site.assigned_unit_ids.erase(
            std::remove(site.assigned_unit_ids.begin(), site.assigned_unit_ids.end(), unit_id),
            site.assigned_unit_ids.end());
    }
}

void InterruptUnitWork(WorldState& world, dbd::UnitState& unit) {
    if (unit.assignment.kind == dbd::JobKind::Flatten ||
        unit.assignment.kind == dbd::JobKind::Build ||
        unit.assignment.kind == dbd::JobKind::Harvest ||
        unit.assignment.kind == dbd::JobKind::Haul) {
        RemoveUnitFromAssignedJobs(world, unit.unit_id);
        ClearUnitAssignment(unit);
        unit.current_order = dbd::UnitOrderType::Idle;
    }
}

float EconomicMoveMultiplier(const WorldState& world, const dbd::UnitState& unit) {
    const auto player_it = world.players.find(unit.owner_player_id);
    if (player_it == world.players.end()) {
        return 1.0f;
    }
    const float penalty = (player_it->second.food_shortage_ratio * 0.25f) + (player_it->second.upkeep_shortage_ratio * 0.20f);
    return std::max(0.55f, 1.0f - penalty);
}

float EconomicLaborMultiplier(const WorldState& world, const dbd::UnitState& unit) {
    const auto player_it = world.players.find(unit.owner_player_id);
    if (player_it == world.players.end()) {
        return 1.0f;
    }
    const float penalty = (player_it->second.food_shortage_ratio * 0.35f) + (player_it->second.upkeep_shortage_ratio * 0.30f);
    return std::max(0.45f, 1.0f - penalty);
}

float EconomicCombatMultiplier(const WorldState& world, const dbd::UnitState& unit) {
    const auto player_it = world.players.find(unit.owner_player_id);
    if (player_it == world.players.end()) {
        return 1.0f;
    }
    const float penalty = (player_it->second.food_shortage_ratio * 0.20f) + (player_it->second.upkeep_shortage_ratio * 0.35f);
    return std::max(0.50f, 1.0f - penalty);
}

float ConsumeStoredResourceAmount(std::vector<dbd::CargoStack>& cargo, float requested_amount) {
    float consumed = 0.0f;
    for (auto& stack : cargo) {
        if (stack.category != dbd::CargoCategory::Resource || stack.amount == 0) {
            continue;
        }
        const float take = std::min<float>(static_cast<float>(stack.amount), requested_amount - consumed);
        stack.amount -= static_cast<std::uint32_t>(take);
        consumed += take;
        if (consumed >= requested_amount) {
            break;
        }
    }
    cargo.erase(
        std::remove_if(
            cargo.begin(),
            cargo.end(),
            [](const dbd::CargoStack& stack) { return stack.amount == 0; }),
        cargo.end());
    return consumed;
}

void AwardConstructionXp(dbd::UnitState& unit, float labor_done) {
    unit.skill_progress.construction_xp += labor_done * 0.12f;
    unit.skill_progress.strength_xp += labor_done * 0.06f;
    while (unit.skill_progress.construction_xp >= 10.0f) {
        unit.skill_progress.construction_xp -= 10.0f;
        ++unit.skills.construction_level;
    }
    while (unit.skill_progress.strength_xp >= 14.0f) {
        unit.skill_progress.strength_xp -= 14.0f;
        ++unit.skills.strength_level;
    }
}

void AwardHarvestXp(dbd::UnitState& unit, float yield_amount) {
    unit.skill_progress.harvest_xp += yield_amount * 0.2f;
    unit.skill_progress.haul_xp += yield_amount * 0.1f;
    while (unit.skill_progress.harvest_xp >= 8.0f) {
        unit.skill_progress.harvest_xp -= 8.0f;
        ++unit.skills.harvest_level;
    }
    while (unit.skill_progress.haul_xp >= 12.0f) {
        unit.skill_progress.haul_xp -= 12.0f;
        ++unit.skills.haul_level;
    }
}

void ClearUnitAssignment(dbd::UnitState& unit) {
    unit.assignment = {};
    unit.current_order = dbd::UnitOrderType::Idle;
}

std::vector<dbd::CargoStack> SplitDestroyedCargo(const std::vector<dbd::CargoStack>& cargo, bool keep_majority) {
    std::vector<dbd::CargoStack> dropped;
    for (const auto& stack : cargo) {
        const std::uint32_t threshold =
            stack.category == dbd::CargoCategory::Resource ? (keep_majority ? 6u : 5u) : (keep_majority ? 3u : 2u);
        if ((stack.amount % 10) < threshold) {
            dropped.push_back(stack);
        }
    }
    return dropped;
}

bool IsFriendlyUnitTarget(const WorldState& world, dbd::Id controller_player_id, dbd::Id target_unit_id) {
    const auto target_it = world.units.find(target_unit_id);
    if (target_it == world.units.end()) {
        return false;
    }

    if (target_it->second.owner_player_id == controller_player_id || target_it->second.controller_player_id == controller_player_id) {
        return true;
    }

    for (const auto& [_, authority] : world.authorities) {
        if (authority.grantee_player_id != controller_player_id) {
            continue;
        }
        if (authority.scope_type == dbd::AuthorityScopeType::Unit && authority.scope_target_id == target_unit_id) {
            return true;
        }
        if (authority.scope_type == dbd::AuthorityScopeType::Squad && authority.scope_target_id == target_it->second.squad_id) {
            return true;
        }
    }

    return false;
}

bool CanAnyAttackerReach(
    const WorldState& world,
    const std::vector<dbd::Id>& attacker_unit_ids,
    const dbd::Vec3& target_position) {
    for (dbd::Id unit_id : attacker_unit_ids) {
        const auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end()) {
            continue;
        }
        const float max_range = MaxAttackRangeFor(unit_it->second);
        if (DistanceSquared(unit_it->second.position, target_position) <= (max_range * max_range)) {
            return true;
        }
    }
    return false;
}

}  // namespace

CommandResult AssignUnitsToSquad(WorldState& world, dbd::Id squad_id, const std::vector<dbd::Id>& unit_ids) {
    auto squad_it = world.squads.find(squad_id);
    if (squad_it == world.squads.end()) {
        return {false, "Squad not found."};
    }

    auto& squad = squad_it->second;
    squad.unit_ids.clear();

    for (dbd::Id unit_id : unit_ids) {
        auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end()) {
            continue;
        }

        unit_it->second.squad_id = squad_id;
        squad.unit_ids.push_back(unit_id);
    }

    if (!squad.unit_ids.empty()) {
        squad.leader_unit_id = squad.unit_ids.front();
    }

    return {true, "Units assigned to squad.", squad_id};
}

CommandResult SetSquadStance(WorldState& world, dbd::Id owner_player_id, dbd::Id squad_id, dbd::SquadStance stance) {
    auto squad_it = world.squads.find(squad_id);
    if (squad_it == world.squads.end()) {
        return {false, "Squad not found."};
    }
    if (squad_it->second.owner_player_id != owner_player_id) {
        return {false, "Only the owning player can change squad stance."};
    }

    squad_it->second.stance = stance;
    return {true, "Squad stance updated.", squad_id};
}

CommandResult GrantAuthority(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id target_player_id,
    dbd::AuthorityScopeType scope_type,
    const std::vector<dbd::Id>& target_ids,
    std::uint32_t permission_mask,
    bool revocable,
    std::uint64_t expires_at_utc_ms) {
    if (target_ids.empty()) {
        return {false, "No authority targets supplied."};
    }

    dbd::Id last_id = 0;
    for (dbd::Id target_id : target_ids) {
        if (scope_type == dbd::AuthorityScopeType::Unit) {
            const auto unit_it = world.units.find(target_id);
            if (unit_it == world.units.end() || unit_it->second.owner_player_id != owner_player_id) {
                return {false, "Authority grant rejected: owner does not own one or more units."};
            }
        }
        if (scope_type == dbd::AuthorityScopeType::Squad) {
            const auto squad_it = world.squads.find(target_id);
            if (squad_it == world.squads.end() || squad_it->second.owner_player_id != owner_player_id) {
                return {false, "Authority grant rejected: owner does not own one or more squads."};
            }
        }
        if (scope_type != dbd::AuthorityScopeType::Unit && scope_type != dbd::AuthorityScopeType::Squad) {
            return {false, "This stage supports only unit or squad delegation."};
        }

        auto& authority = CreateAuthority(world, owner_player_id, target_player_id, scope_type, target_id, permission_mask);
        authority.revocable = revocable;
        authority.expires_at_utc_ms = expires_at_utc_ms;
        last_id = authority.authority_id;
    }

    return {true, "Authority granted.", last_id};
}

CommandResult IssueMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target) {
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Controller does not own command authority for one or more units."};
        }
    }

    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.move_target = target;
        unit.assignment.kind = dbd::JobKind::None;
        unit.assignment.target_entity_id = 0;
        unit.assignment.target_position = target;
        GetOrCreateChunk(world, WorldToChunk(target)).opened_today = true;
    }

    return {true, "Move order accepted."};
}

CommandResult IssueHarvestOrder(WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::Id resource_node_id) {
    if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueGather)) {
        return {false, "Unit command authority denied."};
    }

    auto node_it = world.resource_nodes.find(resource_node_id);
    if (node_it == world.resource_nodes.end()) {
        return {false, "Resource node not found."};
    }

    auto& unit = world.units.at(unit_id);
    auto& node = node_it->second;
    GetOrCreateChunk(world, node.chunk);
    if (node.remaining_amount == 0) {
        return {false, "Resource node depleted."};
    }

    const float throughput = HarvestLaborPerSecond(unit) * EconomicLaborMultiplier(world, unit) * node.extraction_rate;
    const std::uint32_t amount = std::min<std::uint32_t>(static_cast<std::uint32_t>(std::ceil(throughput * 4.0f)), node.remaining_amount);
    node.remaining_amount -= amount;
    unit.current_order = dbd::UnitOrderType::Harvest;
    unit.assignment.kind = dbd::JobKind::Harvest;
    unit.assignment.target_entity_id = resource_node_id;
    unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
    unit.assignment.target_position = node.position;
    unit.cargo.push_back(dbd::CargoStack {
        resource_node_id,
        amount,
        static_cast<float>(amount) * node.richness * 5.0f,
        1.0f + (node.richness * 0.4f),
        dbd::CargoCategory::Resource});
    AwardHarvestXp(unit, static_cast<float>(amount));
    return {true, "Harvest completed.", resource_node_id};
}

CommandResult DeliverCargoToStorage(WorldState& world, dbd::Id unit_id, dbd::Id storage_site_id) {
    auto unit_it = world.units.find(unit_id);
    auto storage_it = world.storage_sites.find(storage_site_id);
    if (unit_it == world.units.end() || storage_it == world.storage_sites.end()) {
        return {false, "Unit or storage site not found."};
    }

    auto& unit = unit_it->second;
    auto& storage = storage_it->second;
    GetOrCreateChunk(world, storage.chunk);
    if (unit.cargo.empty()) {
        return {false, "Unit cargo is empty."};
    }

    storage.stored_resources.insert(storage.stored_resources.end(), unit.cargo.begin(), unit.cargo.end());
    unit.cargo.clear();
    unit.current_order = dbd::UnitOrderType::HaulToStorage;
    unit.assignment.kind = dbd::JobKind::Haul;
    unit.assignment.target_entity_id = storage_site_id;
    unit.assignment.target_position = storage.position;
    unit.skill_progress.haul_xp += 2.5f;
    while (unit.skill_progress.haul_xp >= 12.0f) {
        unit.skill_progress.haul_xp -= 12.0f;
        ++unit.skills.haul_level;
    }
    return {true, "Cargo delivered to storage.", storage_site_id};
}

CommandResult StartFlattenJob(WorldState& world, dbd::Id, const dbd::Vec3& center, float radius, float target_grade) {
    const auto terrain = EvaluateTerrainAt(world, center, radius, target_grade);
    if (terrain.already_buildable) {
        return {false, "Terrain is already buildable at the requested slope tolerance."};
    }

    const auto& job = CreateFlattenJob(world, center, radius, target_grade, terrain.flatten_work_estimate, terrain.average_slope);
    return {true, "Flatten job created.", job.flatten_job_id};
}

CommandResult AssignUnitsToFlattenJob(WorldState& world, dbd::Id controller_player_id, dbd::Id flatten_job_id, const std::vector<dbd::Id>& unit_ids) {
    auto job_it = world.flatten_jobs.find(flatten_job_id);
    if (job_it == world.flatten_jobs.end()) {
        return {false, "Flatten job not found."};
    }

    auto& job = job_it->second;
    GetOrCreateChunk(world, job.chunk);
    job.state = dbd::FlattenJobStateKind::InProgress;

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "One or more assigned units are not controllable."};
        }
        auto& unit = world.units.at(unit_id);
        unit.current_order = dbd::UnitOrderType::FlattenSite;
        unit.assignment.kind = dbd::JobKind::Flatten;
        unit.assignment.target_entity_id = flatten_job_id;
        unit.assignment.target_kind = dbd::AttackTargetKind::ConstructionSite;
        unit.assignment.target_position = job.center;
        if (!ContainsId(job.assigned_unit_ids, unit_id)) {
            job.assigned_unit_ids.push_back(unit_id);
        }
    }

    return {true, "Units assigned to flatten job.", flatten_job_id};
}

CommandResult RemoveUnitsFromFlattenJob(WorldState& world, dbd::Id controller_player_id, dbd::Id flatten_job_id, const std::vector<dbd::Id>& unit_ids) {
    auto job_it = world.flatten_jobs.find(flatten_job_id);
    if (job_it == world.flatten_jobs.end()) {
        return {false, "Flatten job not found."};
    }

    auto& job = job_it->second;
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "One or more units are not controllable."};
        }
        auto& unit = world.units.at(unit_id);
        if (unit.assignment.kind == dbd::JobKind::Flatten && unit.assignment.target_entity_id == flatten_job_id) {
            ClearUnitAssignment(unit);
        }
    }

    EraseIds(job.assigned_unit_ids, unit_ids);
    return {true, "Units removed from flatten job.", flatten_job_id};
}

CommandResult ProgressFlattenJobs(WorldState& world, float delta_seconds) {
    std::ostringstream report;
    std::size_t completed = 0;

    for (auto& [job_id, job] : world.flatten_jobs) {
        if (job.state == dbd::FlattenJobStateKind::Completed || job.state == dbd::FlattenJobStateKind::Canceled) {
            continue;
        }
        auto& chunk = GetOrCreateChunk(world, job.chunk);

        float labor = 0.0f;
        std::vector<dbd::Id> active_units;
        for (dbd::Id unit_id : job.assigned_unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
                continue;
            }
            const float contribution = ConstructionLaborPerSecond(unit_it->second) * EconomicLaborMultiplier(world, unit_it->second) * delta_seconds;
            labor += contribution;
            AwardConstructionXp(unit_it->second, contribution);
            active_units.push_back(unit_id);
        }

        job.assigned_unit_ids = active_units;
        if (labor <= 0.0f) {
            continue;
        }

        job.state = dbd::FlattenJobStateKind::InProgress;
        job.accumulated_labor += labor;
        const float ratio = std::min(1.0f, job.accumulated_labor / std::max(1.0f, job.required_labor));
        job.current_slope = job.initial_slope - ((job.initial_slope - job.target_grade) * ratio);

        if (job.accumulated_labor >= job.required_labor) {
            job.accumulated_labor = job.required_labor;
            job.current_slope = job.target_grade;
            job.state = dbd::FlattenJobStateKind::Completed;
            chunk.flatten_stamps.push_back({job.center, job.radius, job.target_grade});
            ++completed;

            for (auto& [_, site] : world.construction_sites) {
                if (site.flatten_job_id == job_id && site.stage == dbd::ConstructionStage::WaitingForFlatten) {
                    site.stage = dbd::ConstructionStage::UnderConstruction;
                }
            }
        }
    }

    report << completed << " flatten job(s) completed this tick.";
    return {true, report.str()};
}

CommandResult StartConstruction(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::StructureType structure_type,
    const dbd::Vec3& position,
    float footprint_radius) {
    const float allowed_slope = AllowedSlopeForStructure(structure_type);
    const auto terrain = EvaluateTerrainAt(world, position, footprint_radius, allowed_slope);
    const auto time_class = BuildTimeClassFor(structure_type, footprint_radius);

    dbd::Id flatten_job_id = 0;
    bool flattening_required = !terrain.already_buildable;
    if (flattening_required) {
        auto& flatten_job = CreateFlattenJob(world, position, footprint_radius, allowed_slope, terrain.flatten_work_estimate, terrain.average_slope);
        flatten_job_id = flatten_job.flatten_job_id;
    }

    const float labor = BaseLaborForBuildClass(time_class) + (footprint_radius * footprint_radius * 1.75f);
    const auto& site = CreateConstructionSite(
        world,
        owner_player_id,
        structure_type,
        time_class,
        position,
        footprint_radius,
        allowed_slope,
        labor,
        flattening_required,
        flatten_job_id);

    return {true, flattening_required ? "Construction site created and waiting for flattening." : "Construction site created.", site.construction_site_id};
}

CommandResult AssignUnitsToConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids) {
    auto site_it = world.construction_sites.find(construction_site_id);
    if (site_it == world.construction_sites.end()) {
        return {false, "Construction site not found."};
    }

    auto& site = site_it->second;
    GetOrCreateChunk(world, site.chunk);

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "One or more assigned units are not controllable."};
        }
        auto& unit = world.units.at(unit_id);
        unit.current_order = dbd::UnitOrderType::BuildSite;
        unit.assignment.kind = dbd::JobKind::Build;
        unit.assignment.target_entity_id = construction_site_id;
        unit.assignment.target_kind = dbd::AttackTargetKind::ConstructionSite;
        unit.assignment.target_position = site.position;
        if (!ContainsId(site.assigned_unit_ids, unit_id)) {
            site.assigned_unit_ids.push_back(unit_id);
        }
    }

    return {true, "Units assigned to construction site.", construction_site_id};
}

CommandResult RemoveUnitsFromConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids) {
    auto site_it = world.construction_sites.find(construction_site_id);
    if (site_it == world.construction_sites.end()) {
        return {false, "Construction site not found."};
    }

    auto& site = site_it->second;
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "One or more units are not controllable."};
        }
        auto& unit = world.units.at(unit_id);
        if (unit.assignment.kind == dbd::JobKind::Build && unit.assignment.target_entity_id == construction_site_id) {
            ClearUnitAssignment(unit);
        }
    }

    EraseIds(site.assigned_unit_ids, unit_ids);
    return {true, "Units removed from construction site.", construction_site_id};
}

CommandResult ProgressConstructionSites(WorldState& world, float delta_seconds) {
    std::ostringstream report;
    std::size_t completed = 0;

    for (auto it = world.construction_sites.begin(); it != world.construction_sites.end();) {
        auto& site = it->second;
        GetOrCreateChunk(world, site.chunk);
        if (site.stage == dbd::ConstructionStage::Completed || site.stage == dbd::ConstructionStage::Destroyed ||
            site.stage == dbd::ConstructionStage::Canceled) {
            ++it;
            continue;
        }

        if (site.flattening_required && site.flatten_job_id != 0) {
            const auto flatten_it = world.flatten_jobs.find(site.flatten_job_id);
            if (flatten_it != world.flatten_jobs.end() && flatten_it->second.state != dbd::FlattenJobStateKind::Completed) {
                ++it;
                continue;
            }
            site.stage = dbd::ConstructionStage::UnderConstruction;
        }

        float labor = 0.0f;
        std::vector<dbd::Id> active_units;
        for (dbd::Id unit_id : site.assigned_unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
                continue;
            }
            const float contribution = ConstructionLaborPerSecond(unit_it->second) * EconomicLaborMultiplier(world, unit_it->second) * delta_seconds;
            labor += contribution;
            AwardConstructionXp(unit_it->second, contribution);
            active_units.push_back(unit_id);
        }

        site.assigned_unit_ids = active_units;
        if (labor <= 0.0f) {
            ++it;
            continue;
        }

        site.accumulated_labor += labor;
        site.completion_health_ratio = std::min(1.0f, site.accumulated_labor / std::max(1.0f, site.required_labor));

        if (site.accumulated_labor >= site.required_labor) {
            auto& structure = CreateStructure(
                world,
                site.owner_player_id,
                site.structure_type,
                site.time_class,
                site.position,
                site.footprint_radius,
                site.allowed_slope);
            if (site.structure_type == dbd::StructureType::StorageDepot) {
                CreateStorageSite(world, structure.structure_id, site.owner_player_id, site.position);
            }
            site.stage = dbd::ConstructionStage::Completed;
            ++completed;
            it = world.construction_sites.erase(it);
            continue;
        }

        ++it;
    }

    report << completed << " construction site(s) completed this tick.";
    return {true, report.str()};
}

CommandResult DamageConstructionSite(WorldState& world, dbd::Id attacker_player_id, dbd::Id construction_site_id, float damage) {
    auto site_it = world.construction_sites.find(construction_site_id);
    if (site_it == world.construction_sites.end()) {
        return {false, "Construction site not found."};
    }

    auto& site = site_it->second;
    GetOrCreateChunk(world, site.chunk);
    if (site.owner_player_id == attacker_player_id) {
        return {false, "Owner cannot damage own construction site with hostile attack flow."};
    }

    site.health = std::max(0.0f, site.health - damage);
    if (site.health > 0.0f) {
        return {true, "Construction site damaged.", construction_site_id};
    }

    const float salvage_value = (site.accumulated_labor * 0.35f) + (site.max_health * 0.2f);
    CreateDroppedCargo(
        world,
        0,
        site.position,
        {dbd::CargoStack {
            site.construction_site_id,
            static_cast<std::uint32_t>(std::max(1.0f, salvage_value / 10.0f)),
            salvage_value,
            2.0f,
            dbd::CargoCategory::Equipment}});

    site.stage = dbd::ConstructionStage::Destroyed;
    for (dbd::Id unit_id : site.assigned_unit_ids) {
        auto unit_it = world.units.find(unit_id);
        if (unit_it != world.units.end() && unit_it->second.assignment.kind == dbd::JobKind::Build &&
            unit_it->second.assignment.target_entity_id == construction_site_id) {
            ClearUnitAssignment(unit_it->second);
        }
    }
    site.assigned_unit_ids.clear();
    return {true, "Construction site destroyed.", construction_site_id};
}

CommandResult ResolvePermanentUnitDeath(WorldState& world, dbd::Id unit_id) {
    auto unit_it = world.units.find(unit_id);
    if (unit_it == world.units.end()) {
        return {false, "Unit not found."};
    }

    auto& unit = unit_it->second;
    GetOrCreateChunk(world, WorldToChunk(unit.position));
    if (unit.permanently_dead) {
        return {false, "Unit is already permanently dead."};
    }

    unit.alive = false;
    unit.permanently_dead = true;
    ClearUnitAssignment(unit);

    float insurance_payout = 0.0f;
    for (const auto& [_, policy] : world.insurance_policies) {
        if (policy.active && policy.insured_entity_id == unit_id) {
            insurance_payout += ComputeInsurancePayout(policy, 100.0f + CargoValue(unit.cargo));
        }
    }

    const auto dropped = SplitDestroyedCargo(unit.cargo, true);
    if (!dropped.empty()) {
        CreateDroppedCargo(world, unit_id, unit.position, dropped);
    }

    CreateCorpse(world, unit_id, unit.position);
    unit.cargo.clear();
    world.players[unit.owner_player_id].credits += insurance_payout;

    for (auto& [_, squad] : world.squads) {
        squad.unit_ids.erase(std::remove(squad.unit_ids.begin(), squad.unit_ids.end(), unit_id), squad.unit_ids.end());
        if (squad.leader_unit_id == unit_id) {
            squad.leader_unit_id = squad.unit_ids.empty() ? 0 : squad.unit_ids.front();
        }
    }

    return {true, "Permanent death resolved.", unit_id};
}

CommandResult ResolveAttack(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& attacker_unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id) {
    if (attacker_unit_ids.empty()) {
        return {false, "No attackers supplied."};
    }

    dbd::Vec3 target_position {};
    if (target_kind == dbd::AttackTargetKind::Unit) {
        auto target_it = world.units.find(target_entity_id);
        if (target_it == world.units.end()) {
            return {false, "Target unit not found."};
        }
        if (!target_it->second.alive || target_it->second.permanently_dead) {
            return {false, "Target unit is already dead."};
        }
        if (IsFriendlyUnitTarget(world, controller_player_id, target_entity_id)) {
            return {false, "Friendly-fire is blocked in this server slice."};
        }
        target_position = target_it->second.position;
    } else if (target_kind == dbd::AttackTargetKind::ConstructionSite) {
        const auto site_it = world.construction_sites.find(target_entity_id);
        if (site_it == world.construction_sites.end()) {
            return {false, "Target construction site not found."};
        }
        target_position = site_it->second.position;
    } else {
        auto structure_it = world.structures.find(target_entity_id);
        if (structure_it == world.structures.end()) {
            return {false, "Target structure not found."};
        }
        target_position = structure_it->second.position;
    }

    for (dbd::Id unit_id : attacker_unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueAttack)) {
            return {false, "Attack denied for one or more units."};
        }
        auto& unit = world.units.at(unit_id);
        ClearUnitAssignment(unit);
        unit.current_order = dbd::UnitOrderType::AttackTarget;
        unit.assignment.kind = dbd::JobKind::Attack;
        unit.assignment.target_entity_id = target_entity_id;
        unit.assignment.target_kind = target_kind;
        unit.assignment.target_position = target_position;
        unit.move_target = target_position;
    }
    return {true, "Attack order registered.", target_entity_id};
}

CommandResult ProgressUnitMovement(WorldState& world, float delta_seconds) {
    std::size_t moved_units = 0;
    std::size_t delivered_units = 0;

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }

        const bool can_move =
            unit.current_order == dbd::UnitOrderType::Move ||
            unit.current_order == dbd::UnitOrderType::Scout ||
            unit.current_order == dbd::UnitOrderType::AttackTarget ||
            unit.current_order == dbd::UnitOrderType::HaulToStorage ||
            unit.current_order == dbd::UnitOrderType::Retreat;

        ApplyMovementStaminaCost(unit, delta_seconds);

        if (!can_move) {
            continue;
        }

        const float distance = Distance(unit.position, unit.move_target);
        if (distance <= 0.05f) {
            if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                unit.assignment.target_entity_id != 0) {
                const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.target_entity_id);
                if (delivery.ok) {
                    ++delivered_units;
                    ClearUnitAssignment(unit);
                    unit.current_order = dbd::UnitOrderType::Idle;
                }
            } else if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
                continue;
            } else {
                ClearUnitAssignment(unit);
                unit.current_order = dbd::UnitOrderType::Idle;
            }
            continue;
        }

        const float speed = EffectiveMoveSpeed(unit) * EconomicMoveMultiplier(world, unit);
        const float step = std::min(distance, speed * delta_seconds);
        const float inv_distance = 1.0f / std::max(distance, 0.0001f);
        const dbd::Vec3 direction {
            (unit.move_target.x - unit.position.x) * inv_distance,
            (unit.move_target.y - unit.position.y) * inv_distance,
            (unit.move_target.z - unit.position.z) * inv_distance};

        unit.position.x += direction.x * step;
        unit.position.y += direction.y * step;
        unit.position.z += direction.z * step;
        GetOrCreateChunk(world, WorldToChunk(unit.position));
        ++moved_units;

        if (Distance(unit.position, unit.move_target) <= 0.25f) {
            unit.position = unit.move_target;
            if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                unit.assignment.target_entity_id != 0) {
                const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.target_entity_id);
                if (delivery.ok) {
                    ++delivered_units;
                }
            } else if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
                continue;
            }
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
        }
    }

    std::ostringstream message;
    message << "Movement progressed for " << moved_units << " unit(s)";
    if (delivered_units > 0) {
        message << ", " << delivered_units << " auto-delivery completion(s)";
    }
    message << '.';
    return {true, message.str()};
}

CommandResult ProgressCombat(WorldState& world, float delta_seconds) {
    std::size_t engaged_units = 0;
    std::size_t resolved_hits = 0;

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || unit.current_order != dbd::UnitOrderType::AttackTarget) {
            continue;
        }
        if (unit.assignment.kind != dbd::JobKind::Attack || unit.assignment.target_entity_id == 0) {
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
            continue;
        }

        dbd::Vec3 target_position = unit.assignment.target_position;
        bool target_valid = true;

        if (unit.assignment.target_kind == dbd::AttackTargetKind::Unit) {
            const auto target_it = world.units.find(unit.assignment.target_entity_id);
            if (target_it == world.units.end() || !target_it->second.alive || target_it->second.permanently_dead ||
                target_it->second.owner_player_id == unit.owner_player_id) {
                target_valid = false;
            } else {
                target_position = target_it->second.position;
            }
        } else if (unit.assignment.target_kind == dbd::AttackTargetKind::ConstructionSite) {
            const auto site_it = world.construction_sites.find(unit.assignment.target_entity_id);
            if (site_it == world.construction_sites.end() || site_it->second.stage == dbd::ConstructionStage::Destroyed ||
                site_it->second.stage == dbd::ConstructionStage::Completed) {
                target_valid = false;
            } else {
                target_position = site_it->second.position;
            }
        } else {
            const auto structure_it = world.structures.find(unit.assignment.target_entity_id);
            if (structure_it == world.structures.end()) {
                target_valid = false;
            } else {
                target_position = structure_it->second.position;
            }
        }

        if (!target_valid) {
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
            continue;
        }

        unit.assignment.target_position = target_position;
        unit.move_target = target_position;
        ++engaged_units;

        const float range = MaxAttackRangeFor(unit);
        if (DistanceSquared(unit.position, target_position) > (range * range)) {
            if (world.squads.find(unit.squad_id) != world.squads.end() &&
                world.squads.at(unit.squad_id).stance == dbd::SquadStance::Defensive &&
                !EnemySeenNearby(world, unit)) {
                ClearUnitAssignment(unit);
                unit.current_order = dbd::UnitOrderType::Idle;
            }
            continue;
        }

        const float damage = BaseCombatDamagePerSecond(unit) * EconomicCombatMultiplier(world, unit) * delta_seconds;
        ++resolved_hits;

        if (unit.assignment.target_kind == dbd::AttackTargetKind::Unit) {
            auto target_it = world.units.find(unit.assignment.target_entity_id);
            if (target_it == world.units.end()) {
                ClearUnitAssignment(unit);
                unit.current_order = dbd::UnitOrderType::Idle;
                continue;
            }

            auto& target = target_it->second;
            target.health = std::max(0.0f, target.health - damage);
            InterruptUnitWork(world, target);
            if (target.health <= 0.0f) {
                ResolvePermanentUnitDeath(world, target.unit_id);
                ClearUnitAssignment(unit);
                unit.current_order = dbd::UnitOrderType::Idle;
            }
            continue;
        }

        if (unit.assignment.target_kind == dbd::AttackTargetKind::ConstructionSite) {
            DamageConstructionSite(world, unit.owner_player_id, unit.assignment.target_entity_id, damage);
            continue;
        }

        auto structure_it = world.structures.find(unit.assignment.target_entity_id);
        if (structure_it == world.structures.end()) {
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
            continue;
        }

        auto& structure = structure_it->second;
        structure.health = std::max(0.0f, structure.health - damage);
        if (structure.health <= 0.0f) {
            float insurance_payout = 0.0f;
            for (const auto& [_, policy] : world.insurance_policies) {
                if (policy.active && policy.insured_entity_id == structure.structure_id) {
                    insurance_payout += ComputeInsurancePayout(policy, structure.max_health * 2.0f);
                }
            }
            world.players[structure.owner_player_id].credits += insurance_payout;

            for (auto storage_it = world.storage_sites.begin(); storage_it != world.storage_sites.end();) {
                if (storage_it->second.structure_id != structure.structure_id) {
                    ++storage_it;
                    continue;
                }
                const auto dropped = SplitDestroyedCargo(storage_it->second.stored_resources, false);
                if (!dropped.empty()) {
                    CreateDroppedCargo(world, 0, structure.position, dropped);
                }
                storage_it = world.storage_sites.erase(storage_it);
            }

            CreateDroppedCargo(
                world,
                0,
                structure.position,
                {dbd::CargoStack {
                    structure.structure_id,
                    static_cast<std::uint32_t>(std::max(1.0f, structure.max_health / 40.0f)),
                    structure.max_health * 0.45f,
                    3.0f,
                    dbd::CargoCategory::Equipment}});

            world.players[structure.owner_player_id].structure_ids.erase(
                std::remove(
                    world.players[structure.owner_player_id].structure_ids.begin(),
                    world.players[structure.owner_player_id].structure_ids.end(),
                    structure.structure_id),
                world.players[structure.owner_player_id].structure_ids.end());
            world.structures.erase(structure.structure_id);
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
        }
    }

    std::ostringstream message;
    message << "Combat progressed for " << engaged_units << " attacking unit(s), " << resolved_hits << " hit resolution(s).";
    return {true, message.str()};
}

CommandResult ProcessEconomicSettlement(WorldState& world, float delta_seconds) {
    constexpr float kSettlementSeconds = 10.0f;
    std::size_t settled_players = 0;
    if (delta_seconds < kSettlementSeconds) {
        return {true, "Economic settlement skipped: waiting for settlement interval."};
    }

    for (auto& [player_id, player] : world.players) {
        float upkeep = 0.0f;
        float tax = 0.0f;
        float food = 0.0f;
        std::size_t living_units = 0;

        for (dbd::Id unit_id : player.unit_ids) {
            const auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
                continue;
            }
            ++living_units;
            upkeep += unit_it->second.upkeep_cost;
            tax += unit_it->second.tax_weight * 0.6f;
            food += unit_it->second.food_demand;
        }

        std::size_t active_structures = 0;
        for (dbd::Id structure_id : player.structure_ids) {
            if (world.structures.find(structure_id) != world.structures.end()) {
                ++active_structures;
            }
        }

        player.upkeep_load = upkeep;
        player.tax_load = tax;
        player.food_load = food;
        player.complexity_load = static_cast<float>(living_units) + (static_cast<float>(active_structures) * 0.5f);

        float food_available = 0.0f;
        for (auto& [_, storage] : world.storage_sites) {
            if (storage.owner_player_id != player_id) {
                continue;
            }
            food_available += ConsumeStoredResourceAmount(storage.stored_resources, food - food_available);
            if (food_available >= food) {
                break;
            }
        }

        player.food_shortage_ratio = food > 0.0f ? std::max(0.0f, food - food_available) / food : 0.0f;

        const float total_credit_cost = upkeep + tax;
        const float paid = std::min(player.credits, total_credit_cost);
        player.credits -= paid;
        player.upkeep_shortage_ratio = total_credit_cost > 0.0f ? std::max(0.0f, total_credit_cost - paid) / total_credit_cost : 0.0f;

        for (dbd::Id unit_id : player.unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
                continue;
            }

            if (player.food_shortage_ratio > 0.0f) {
                unit_it->second.stamina = std::max(0.0f, unit_it->second.stamina - (12.0f * player.food_shortage_ratio));
            }
            if (player.upkeep_shortage_ratio > 0.0f) {
                unit_it->second.health = std::max(15.0f, unit_it->second.health - (4.0f * player.upkeep_shortage_ratio));
            }
        }

        ++settled_players;
    }

    std::ostringstream message;
    message << "Economic settlement processed for " << settled_players << " player(s).";
    return {true, message.str()};
}

CommandResult EvaluateAutomation(WorldState& world, float delta_seconds) {
    (void)delta_seconds;
    std::size_t actions_applied = 0;

    for (auto& [_, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }

        const float health_ratio = unit.health / std::max(1.0f, unit.max_health);
        const float cargo_ratio = CargoFillRatio(unit);
        const bool enemy_seen = EnemySeenNearby(world, unit);

        for (const auto& rule : unit.automation_rules) {
            if (!rule.enabled) {
                continue;
            }

            bool triggered = false;
            switch (rule.trigger) {
                case dbd::AutomationTrigger::LowHealth:
                    triggered = health_ratio <= rule.threshold;
                    break;
                case dbd::AutomationTrigger::InventoryHeavy:
                    triggered = cargo_ratio >= rule.threshold || IsOverburdened(unit);
                    break;
                case dbd::AutomationTrigger::InventoryFull:
                    triggered = cargo_ratio >= rule.threshold;
                    break;
                case dbd::AutomationTrigger::EnemySeen:
                    triggered = enemy_seen;
                    break;
            }

            if (!triggered) {
                continue;
            }

            switch (rule.action) {
                case dbd::AutomationAction::Retreat:
                    unit.current_order = dbd::UnitOrderType::Retreat;
                    unit.assignment.kind = dbd::JobKind::Retreat;
                    unit.assignment.target_entity_id = 0;
                    unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
                    unit.move_target = ComputeRetreatTarget(world, unit);
                    unit.assignment.target_position = unit.move_target;
                    ++actions_applied;
                    break;
                case dbd::AutomationAction::ReturnToStorage: {
                    const auto nearest_storage = FindNearestStorageSite(world, unit.owner_player_id, unit.position);
                    if (nearest_storage.has_value()) {
                        auto storage_it = world.storage_sites.find(*nearest_storage);
                        if (storage_it != world.storage_sites.end()) {
                            unit.current_order = dbd::UnitOrderType::HaulToStorage;
                            unit.assignment.kind = dbd::JobKind::Haul;
                            unit.assignment.target_entity_id = *nearest_storage;
                            unit.assignment.target_position = storage_it->second.position;
                            unit.move_target = storage_it->second.position;
                            ++actions_applied;
                        }
                    }
                    break;
                }
                case dbd::AutomationAction::HoldPosition:
                    unit.current_order = dbd::UnitOrderType::Idle;
                    ClearUnitAssignment(unit);
                    ++actions_applied;
                    break;
                case dbd::AutomationAction::AttackNearestEnemy:
                    if (enemy_seen && unit.stamina > 5.0f) {
                        const auto nearest_enemy = FindNearestEnemyUnit(world, unit);
                        if (!nearest_enemy.has_value()) {
                            break;
                        }
                        const auto enemy_it = world.units.find(*nearest_enemy);
                        if (enemy_it == world.units.end()) {
                            break;
                        }
                        unit.current_order = dbd::UnitOrderType::AttackTarget;
                        unit.assignment.kind = dbd::JobKind::Attack;
                        unit.assignment.target_entity_id = *nearest_enemy;
                        unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
                        unit.assignment.target_position = enemy_it->second.position;
                        unit.move_target = enemy_it->second.position;
                        ++actions_applied;
                    }
                    break;
            }
            break;
        }
    }

    for (auto& [_, squad] : world.squads) {
        if (squad.automation_rules.empty()) {
            continue;
        }
        for (dbd::Id unit_id : squad.unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
                continue;
            }
            if (squad.stance == dbd::SquadStance::Retreat) {
                unit_it->second.current_order = dbd::UnitOrderType::Retreat;
                unit_it->second.assignment.kind = dbd::JobKind::Retreat;
                unit_it->second.move_target = ComputeRetreatTarget(world, unit_it->second);
                unit_it->second.assignment.target_position = unit_it->second.move_target;
            } else if (squad.stance == dbd::SquadStance::Defensive &&
                       unit_it->second.current_order == dbd::UnitOrderType::AttackTarget &&
                       !EnemySeenNearby(world, unit_it->second)) {
                unit_it->second.current_order = dbd::UnitOrderType::Idle;
                ClearUnitAssignment(unit_it->second);
            }
        }
    }

    std::ostringstream message;
    message << "Automation evaluated for " << world.units.size() << " unit(s), " << actions_applied << " action(s) applied.";
    return {true, message.str()};
}

float ComputeInsurancePayout(const dbd::InsurancePolicyState& policy, float realized_loss_value) {
    const float covered_value = std::min(policy.insured_value, realized_loss_value);
    return covered_value * policy.payout_rate;
}

}  // namespace dbd_server
