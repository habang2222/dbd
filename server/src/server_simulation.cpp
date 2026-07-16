#include "dbd_server/server_simulation.hpp"
#include "dbd_server/tactical_state.hpp"

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

bool PlayerExists(const WorldState& world, dbd::Id player_id) {
    return player_id != 0 && world.players.find(player_id) != world.players.end();
}

dbd::Id HarvestItemIdForRiskBand(dbd::RegionRiskBand risk_band) {
    switch (risk_band) {
        case dbd::RegionRiskBand::Low:
            return 91'001; // Basic Wood
        case dbd::RegionRiskBand::Medium:
            return 91'002; // Stone Block
        case dbd::RegionRiskBand::High:
            return 91'003; // Iron Fitting
    }
    return 91'001;
}

dbd::CargoStack CreateHarvestCargoStack(const dbd::ResourceNodeState& node, std::uint32_t amount) {
    const dbd::Id item_id = HarvestItemIdForRiskBand(node.risk_band);
    float unit_weight = 1.0f + (node.richness * 0.4f);
    float value_per_unit = node.richness * 5.0f;
    if (const auto* definition = dbd::FindItemDefinition(item_id)) {
        unit_weight = definition->unit_weight;
        value_per_unit = definition->base_value * std::max(0.75f, node.richness);
    }
    return dbd::CargoStack {
        item_id,
        amount,
        static_cast<float>(amount) * value_per_unit,
        unit_weight,
        dbd::CargoCategory::Resource};
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

float AutoConstructionProgressPerSecond(const dbd::ConstructionSiteState& site) {
    switch (site.time_class) {
        case dbd::BuildTimeClass::SmallFast:
            return 8.5f;
        case dbd::BuildTimeClass::MediumStandard:
            return 5.0f;
        case dbd::BuildTimeClass::LargeSlow:
        default:
            return 2.75f;
    }
}

float HarvestLaborPerSecond(const dbd::UnitState& unit) {
    return 1.0f + (unit.skills.harvest_level * 0.85f) + (unit.skills.strength_level * 0.25f);
}

float BaseCombatDamagePerSecond(const dbd::UnitState& unit) {
    float base = 3.0f + (unit.skills.combat_level * 1.4f) + (unit.skills.strength_level * 0.4f);
    switch (unit.combat_band) {
        case dbd::CombatRangeBand::Short:
            return base * 1.45f;
        case dbd::CombatRangeBand::Long:
            return base * 0.7f;
        case dbd::CombatRangeBand::Mid:
        default:
            return base * 1.0f;
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
    const float skill_bonus = unit.skills.combat_level * 0.55f;
    switch (unit.combat_band) {
        case dbd::CombatRangeBand::Short:
            return 4.5f + skill_bonus;
        case dbd::CombatRangeBand::Long:
            return 18.0f + skill_bonus;
        case dbd::CombatRangeBand::Mid:
        default:
            return 10.5f + skill_bonus;
    }
}

void ClearUnitAssignment(dbd::UnitState& unit);
void RemoveUnitFromAssignedJobs(WorldState& world, dbd::Id unit_id);

float VisionRangeFor(const dbd::UnitState& unit) {
    return 14.0f + (unit.skills.survival_level * 0.75f);
}

constexpr std::uint64_t kFreshContactTicks = 8000;
constexpr std::uint64_t kContactRetentionTicks = 120000;
constexpr std::size_t kMaxQueuedOrders = 2;

std::optional<dbd::KnownContactState> FindKnownContact(
    const WorldState& world,
    dbd::Id observing_player_id,
    dbd::AttackTargetKind target_kind,
    dbd::Id target_entity_id) {
    const auto player_contacts_it = world.known_contacts.find(observing_player_id);
    if (player_contacts_it == world.known_contacts.end()) {
        return std::nullopt;
    }
    const auto contact_it = player_contacts_it->second.find(target_entity_id);
    if (contact_it == player_contacts_it->second.end() || contact_it->second.target_kind != target_kind) {
        return std::nullopt;
    }
    return contact_it->second;
}

bool HasFreshContact(
    const WorldState& world,
    dbd::Id observing_player_id,
    dbd::AttackTargetKind target_kind,
    dbd::Id target_entity_id) {
    const auto contact = FindKnownContact(world, observing_player_id, target_kind, target_entity_id);
    if (!contact.has_value()) {
        return false;
    }
    return contact->currently_visible || world.tick <= contact->last_seen_tick + kFreshContactTicks;
}

void MoveUnitsToLastSeenContact(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::KnownContactState& contact) {
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            continue;
        }
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        unit.queued_orders.clear();
        unit.current_order = dbd::UnitOrderType::Move;
        unit.assignment.kind = dbd::JobKind::Scout;
        unit.assignment.target_entity_id = contact.target_entity_id;
        unit.assignment.target_kind = contact.target_kind;
        unit.assignment.target_position = contact.position;
        unit.assignment.delegated = true;
        unit.move_target = contact.position;
        GetOrCreateChunk(world, WorldToChunk(contact.position)).opened_today = true;
    }
}

dbd::Vec3 FormationOffsetTarget(const dbd::Vec3& target, std::size_t index, std::size_t count) {
    if (count <= 1) {
        return target;
    }
    const float spacing = 2.4f;
    float x_offset = 0.0f;
    float z_offset = 0.0f;
    if (count == 2) {
        x_offset = index == 0 ? -spacing * 0.5f : spacing * 0.5f;
    } else if (count == 3) {
        if (index == 0) {
            z_offset = -spacing * 0.55f;
        } else {
            x_offset = index == 1 ? -spacing * 0.55f : spacing * 0.55f;
            z_offset = spacing * 0.55f;
        }
    } else if (count == 4) {
        x_offset = (index % 2 == 0) ? -spacing * 0.55f : spacing * 0.55f;
        z_offset = (index < 2) ? -spacing * 0.55f : spacing * 0.55f;
    } else {
        const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        const int row = static_cast<int>(index) / columns;
        const int column = static_cast<int>(index) % columns;
        const float width = static_cast<float>(std::min<std::size_t>(count, static_cast<std::size_t>(columns)) - 1) * spacing;
        x_offset = (static_cast<float>(column) * spacing) - (width * 0.5f);
        z_offset = static_cast<float>(row) * spacing;
    }
    return {target.x + x_offset, target.y, target.z + z_offset};
}

void ApplyQueuedOrder(dbd::UnitState& unit, const dbd::QueuedOrderState& queued) {
    unit.current_order = queued.order_type;
    unit.assignment = queued.assignment;
    unit.move_target = queued.move_target;
    unit.queue_interrupt_reason.clear();
    unit.tactical_state = "Queued order started";
}

bool TryStartNextQueuedOrder(dbd::UnitState& unit) {
    if (unit.queued_orders.empty()) {
        return false;
    }
    const auto queued = unit.queued_orders.front();
    unit.queued_orders.erase(unit.queued_orders.begin());
    ApplyQueuedOrder(unit, queued);
    return true;
}

bool QueueOrder(dbd::UnitState& unit, const dbd::QueuedOrderState& queued) {
    const bool replaced_oldest = unit.queued_orders.size() >= kMaxQueuedOrders;
    if (unit.queued_orders.size() >= kMaxQueuedOrders) {
        unit.queued_orders.erase(unit.queued_orders.begin());
    }
    unit.queued_orders.push_back(queued);
    unit.queue_interrupt_reason.clear();
    unit.tactical_state = "Order queued";
    return replaced_oldest;
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

bool CargoAtOrOverCapacity(const dbd::UnitState& unit) {
    return CargoWeight(unit.cargo) >= std::max(1.0f, unit.carry_capacity);
}

bool IsOverburdened(const dbd::UnitState& unit) {
    return CargoWeight(unit.cargo) > unit.overburden_threshold;
}

std::optional<dbd::Vec3> HaulSourcePosition(const WorldState& world, dbd::HaulRouteSourceKind source_kind, dbd::Id source_id) {
    if (source_kind == dbd::HaulRouteSourceKind::ResourceNode) {
        const auto node_it = world.resource_nodes.find(source_id);
        if (node_it == world.resource_nodes.end() || node_it->second.remaining_amount == 0) {
            return std::nullopt;
        }
        return node_it->second.position;
    }
    if (source_kind == dbd::HaulRouteSourceKind::DroppedCargo) {
        const auto drop_it = world.dropped_cargo.find(source_id);
        if (drop_it == world.dropped_cargo.end() || drop_it->second.cargo.empty()) {
            return std::nullopt;
        }
        return drop_it->second.position;
    }
    return std::nullopt;
}

int RouteThreatRank(const std::string& level) {
    if (level == "critical") {
        return 2;
    }
    if (level == "threatened") {
        return 1;
    }
    return 0;
}

void RaiseRouteThreat(dbd::UnitState& unit, const std::string& level, dbd::Id enemy_id) {
    if (RouteThreatRank(level) <= RouteThreatRank(unit.route_threat_level)) {
        return;
    }
    unit.route_threat_level = level;
    unit.route_threat_enemy_id = enemy_id;
}

dbd::Vec3 UnitSelectionCentroid(const WorldState& world, const std::vector<dbd::Id>& unit_ids) {
    dbd::Vec3 center {};
    std::size_t count = 0;
    for (dbd::Id unit_id : unit_ids) {
        const auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
            continue;
        }
        center.x += unit_it->second.position.x;
        center.y += unit_it->second.position.y;
        center.z += unit_it->second.position.z;
        ++count;
    }
    if (count == 0) {
        return center;
    }
    center.x /= static_cast<float>(count);
    center.y /= static_cast<float>(count);
    center.z /= static_cast<float>(count);
    return center;
}

std::optional<dbd::Id> FindBestThreatenedRouteHauler(
    const WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& responder_unit_ids) {
    const dbd::Vec3 responder_center = UnitSelectionCentroid(world, responder_unit_ids);
    dbd::Id best_unit_id = 0;
    int best_rank = 0;
    float best_distance_sq = std::numeric_limits<float>::max();

    for (const auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || !unit.assignment.route_active) {
            continue;
        }
        if (unit.owner_player_id != controller_player_id && unit.controller_player_id != controller_player_id) {
            continue;
        }
        const int rank = RouteThreatRank(unit.route_threat_level);
        if (rank <= 0) {
            continue;
        }
        const float distance_sq = DistanceSquared(responder_center, unit.position);
        if (rank > best_rank || (rank == best_rank && distance_sq < best_distance_sq)) {
            best_rank = rank;
            best_distance_sq = distance_sq;
            best_unit_id = unit_id;
        }
    }

    if (best_unit_id == 0) {
        return std::nullopt;
    }
    return best_unit_id;
}

std::optional<dbd::Id> NearestOwnedStorageForUnit(const WorldState& world, const dbd::UnitState& unit) {
    return FindNearestStorageSite(world, unit.owner_player_id, unit.position);
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
            speed *= unit.stamina > 20.0f ? 1.65f : 1.05f;
            break;
        case dbd::UnitOrderType::HaulToStorage:
            speed *= 0.95f;
            break;
        case dbd::UnitOrderType::Escort:
            speed *= 1.0f;
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
        speed *= unit.current_order == dbd::UnitOrderType::Retreat ? 0.62f : 0.78f;
    }
    if (unit.current_order == dbd::UnitOrderType::Retreat && unit.stamina <= 5.0f) {
        speed *= 0.7f;
    }

    return std::max(0.5f, speed);
}

bool IsMoveTargetInsideObstacle(const dbd::Vec3& target, const dbd::Vec3& obstacle_center, float obstacle_radius) {
    return DistanceSquared(target, obstacle_center) <= ((obstacle_radius + 0.75f) * (obstacle_radius + 0.75f));
}

bool MovementWouldCollide(
    const WorldState& world,
    dbd::Id moving_unit_id,
    const dbd::Vec3& current_position,
    const dbd::Vec3& next_position,
    const dbd::Vec3& move_target) {
    constexpr float kUnitBodyRadius = 1.45f;
    constexpr float kUnitSeparationRadius = 2.15f;

    for (const auto& [other_id, other] : world.units) {
        if (other_id == moving_unit_id || !other.alive || other.permanently_dead) {
            continue;
        }
        if (IsMoveTargetInsideObstacle(move_target, other.position, kUnitSeparationRadius)) {
            continue;
        }
        if (DistanceSquared(current_position, other.position) <= (kUnitSeparationRadius * kUnitSeparationRadius)) {
            continue;
        }
        if (DistanceSquared(next_position, other.position) <= (kUnitSeparationRadius * kUnitSeparationRadius)) {
            return true;
        }
    }

    for (const auto& [_, structure] : world.structures) {
        const float radius = std::max(3.0f, structure.footprint_radius);
        if (IsMoveTargetInsideObstacle(move_target, structure.position, radius)) {
            continue;
        }
        if (DistanceSquared(current_position, structure.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            continue;
        }
        if (DistanceSquared(next_position, structure.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            return true;
        }
    }

    for (const auto& [_, site] : world.construction_sites) {
        const float radius = std::max(3.0f, site.footprint_radius);
        if (IsMoveTargetInsideObstacle(move_target, site.position, radius)) {
            continue;
        }
        if (DistanceSquared(current_position, site.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            continue;
        }
        if (DistanceSquared(next_position, site.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            return true;
        }
    }

    for (const auto& [_, storage] : world.storage_sites) {
        const float radius = 3.25f;
        if (IsMoveTargetInsideObstacle(move_target, storage.position, radius)) {
            continue;
        }
        if (DistanceSquared(current_position, storage.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            continue;
        }
        if (DistanceSquared(next_position, storage.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            return true;
        }
    }

    for (const auto& [_, node] : world.resource_nodes) {
        const float radius = 2.75f;
        if (IsMoveTargetInsideObstacle(move_target, node.position, radius)) {
            continue;
        }
        if (DistanceSquared(current_position, node.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            continue;
        }
        if (DistanceSquared(next_position, node.position) <= ((radius + kUnitBodyRadius) * (radius + kUnitBodyRadius))) {
            return true;
        }
    }

    return false;
}

std::optional<dbd::Vec3> FindSimpleDetourStep(
    const WorldState& world,
    dbd::Id moving_unit_id,
    const dbd::Vec3& position,
    const dbd::Vec3& move_target,
    const dbd::Vec3& direction,
    float step) {
    const dbd::Vec3 side {-direction.z, 0.0f, direction.x};
    const float detour_step = std::max(step, 0.85f);
    const float forward_step = step * 0.45f;
    const dbd::Vec3 candidates[] {
        {position.x + (side.x * detour_step) + (direction.x * forward_step), position.y, position.z + (side.z * detour_step) + (direction.z * forward_step)},
        {position.x - (side.x * detour_step) + (direction.x * forward_step), position.y, position.z - (side.z * detour_step) + (direction.z * forward_step)},
        {position.x + (side.x * detour_step * 1.8f), position.y, position.z + (side.z * detour_step * 1.8f)},
        {position.x - (side.x * detour_step * 1.8f), position.y, position.z - (side.z * detour_step * 1.8f)}
    };

    std::optional<dbd::Vec3> best;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto& candidate : candidates) {
        if (MovementWouldCollide(world, moving_unit_id, position, candidate, move_target)) {
            continue;
        }
        const float distance_to_target = DistanceSquared(candidate, move_target);
        if (distance_to_target < best_distance) {
            best_distance = distance_to_target;
            best = candidate;
        }
    }
    return best;
}

bool ConstructionFootprintOverlapsWorld(
    const WorldState& world,
    const dbd::Vec3& position,
    float footprint_radius) {
    const float requested_radius = std::max(1.0f, footprint_radius);
    constexpr float kBuildClearance = 1.25f;

    for (const auto& [_, structure] : world.structures) {
        const float radius = std::max(1.0f, structure.footprint_radius);
        const float required = requested_radius + radius + kBuildClearance;
        if (DistanceSquared(position, structure.position) <= required * required) {
            return true;
        }
    }

    for (const auto& [_, site] : world.construction_sites) {
        if (site.stage == dbd::ConstructionStage::Destroyed || site.stage == dbd::ConstructionStage::Canceled) {
            continue;
        }
        const float radius = std::max(1.0f, site.footprint_radius);
        const float required = requested_radius + radius + kBuildClearance;
        if (DistanceSquared(position, site.position) <= required * required) {
            return true;
        }
    }

    for (const auto& [_, node] : world.resource_nodes) {
        const float radius = 2.75f;
        const float required = requested_radius + radius + kBuildClearance;
        if (DistanceSquared(position, node.position) <= required * required) {
            return true;
        }
    }

    return false;
}

std::optional<dbd::Id> FindNearestEnemyUnit(WorldState& world, const dbd::UnitState& unit) {
    float best_distance = std::numeric_limits<float>::max();
    std::optional<dbd::Id> best_enemy;
    const float vision_range = VisionRangeFor(unit);
    const float vision_range_sq = vision_range * vision_range;

    for (dbd::Id other_id : FindNearbyUnitIds(world, unit.position, vision_range)) {
        const auto other_it = world.units.find(other_id);
        if (other_it == world.units.end()) {
            continue;
        }
        const auto& other = other_it->second;
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

std::optional<dbd::Id> FindNearestEnemyUnitNearPoint(
    WorldState& world,
    dbd::Id owner_player_id,
    const dbd::Vec3& point,
    float radius) {
    float best_distance = std::numeric_limits<float>::max();
    std::optional<dbd::Id> best_enemy;
    const float radius_sq = radius * radius;

    for (dbd::Id other_id : FindNearbyUnitIds(world, point, radius)) {
        const auto other_it = world.units.find(other_id);
        if (other_it == world.units.end()) {
            continue;
        }
        const auto& other = other_it->second;
        if (!other.alive || other.permanently_dead || other.owner_player_id == owner_player_id) {
            continue;
        }
        const float distance_sq = DistanceSquared(other.position, point);
        if (distance_sq > radius_sq || distance_sq >= best_distance) {
            continue;
        }
        best_distance = distance_sq;
        best_enemy = other_id;
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

bool EnemySeenNearby(WorldState& world, const dbd::UnitState& unit) {
    const float vision_range = VisionRangeFor(unit);
    const float vision_range_sq = vision_range * vision_range;
    for (dbd::Id other_id : FindNearbyUnitIds(world, unit.position, vision_range)) {
        const auto other_it = world.units.find(other_id);
        if (other_it == world.units.end()) {
            continue;
        }
        const auto& other = other_it->second;
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

std::optional<dbd::Vec3> ResolveGuardAnchor(const WorldState& world, const dbd::UnitState& unit) {
    if (unit.assignment.target_entity_id == 0) {
        return unit.assignment.target_position;
    }

    if (unit.assignment.target_kind == dbd::AttackTargetKind::Unit) {
        const auto target_it = world.units.find(unit.assignment.target_entity_id);
        if (target_it == world.units.end() || !target_it->second.alive || target_it->second.permanently_dead ||
            target_it->second.owner_player_id != unit.owner_player_id) {
            return std::nullopt;
        }
        return target_it->second.position;
    }

    if (unit.assignment.target_kind == dbd::AttackTargetKind::ConstructionSite) {
        const auto site_it = world.construction_sites.find(unit.assignment.target_entity_id);
        if (site_it == world.construction_sites.end() || site_it->second.stage == dbd::ConstructionStage::Destroyed ||
            site_it->second.stage == dbd::ConstructionStage::Completed) {
            return std::nullopt;
        }
        return site_it->second.position;
    }

    const auto structure_it = world.structures.find(unit.assignment.target_entity_id);
    if (structure_it == world.structures.end()) {
        return std::nullopt;
    }
    return structure_it->second.position;
}

void ClearUnitAssignment(dbd::UnitState& unit);

void RemoveUnitFromAssignedJobs(WorldState& world, dbd::Id unit_id) {
    for (auto& [_, job] : world.flatten_jobs) {
        job.assigned_unit_ids.erase(
            std::remove(job.assigned_unit_ids.begin(), job.assigned_unit_ids.end(), unit_id),
            job.assigned_unit_ids.end());
    }
}

void InterruptUnitWork(WorldState& world, dbd::UnitState& unit) {
    if (unit.assignment.kind == dbd::JobKind::Flatten ||
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

float StoredItemAmountForPlayer(const WorldState& world, dbd::Id player_id, dbd::Id item_id) {
    float total = 0.0f;
    for (const auto& [_, storage] : world.storage_sites) {
        if (storage.owner_player_id != player_id) {
            continue;
        }
        for (const auto& stack : storage.stored_resources) {
            if (stack.resource_id == item_id) {
                total += static_cast<float>(stack.amount);
            }
        }
    }
    return total;
}

float ConsumeStoredItemAmount(WorldState& world, dbd::Id player_id, dbd::Id item_id, float requested_amount) {
    float consumed = 0.0f;
    for (auto& [_, storage] : world.storage_sites) {
        if (storage.owner_player_id != player_id) {
            continue;
        }
        for (auto& stack : storage.stored_resources) {
            if (stack.resource_id != item_id || stack.amount == 0 || consumed >= requested_amount) {
                continue;
            }
            const float take = std::min<float>(static_cast<float>(stack.amount), requested_amount - consumed);
            stack.amount -= static_cast<std::uint32_t>(take);
            consumed += take;
            if (consumed >= requested_amount) {
                break;
            }
        }
        storage.stored_resources.erase(
            std::remove_if(storage.stored_resources.begin(), storage.stored_resources.end(),
                [](const dbd::CargoStack& stack) { return stack.amount == 0; }),
            storage.stored_resources.end());
        if (consumed >= requested_amount) {
            break;
        }
    }
    return consumed;
}

void AddStoredItem(WorldState& world, dbd::Id player_id, dbd::Id item_id, std::uint32_t amount) {
    if (amount == 0) {
        return;
    }
    const auto* definition = dbd::FindItemDefinition(item_id);
    const float unit_weight = definition != nullptr ? definition->unit_weight : 1.0f;
    const float base_value = definition != nullptr ? definition->base_value : unit_weight;
    const dbd::CargoCategory category =
        definition != nullptr && definition->category == dbd::ItemCategory::Equipment
            ? dbd::CargoCategory::Equipment
            : dbd::CargoCategory::Resource;

    for (auto& [_, storage] : world.storage_sites) {
        if (storage.owner_player_id != player_id) {
            continue;
        }
        storage.stored_resources.push_back(dbd::CargoStack {item_id, amount, base_value * static_cast<float>(amount), unit_weight, category});
        return;
    }
}

float ConsumeOwnedRepairMaterials(WorldState& world, dbd::Id owner_player_id, float requested_amount) {
    float consumed = 0.0f;
    for (auto& [_, storage] : world.storage_sites) {
        if (storage.owner_player_id != owner_player_id) {
            continue;
        }
        consumed += ConsumeStoredResourceAmount(storage.stored_resources, requested_amount - consumed);
        if (consumed >= requested_amount) {
            break;
        }
    }
    return consumed;
}

float ConsumeNearbyRepairSupply(WorldState& world, dbd::Id owner_player_id, const dbd::Vec3& target_position, float requested_amount) {
    float consumed = 0.0f;
    std::vector<dbd::Id> empty_drops;
    constexpr float kRepairSupplyRadius = 8.0f;
    for (auto& [drop_id, drop] : world.dropped_cargo) {
        if (DistanceSquared(drop.position, target_position) > kRepairSupplyRadius * kRepairSupplyRadius) {
            continue;
        }
        consumed += ConsumeStoredResourceAmount(drop.cargo, requested_amount - consumed);
        if (drop.cargo.empty()) {
            empty_drops.push_back(drop_id);
        }
        if (consumed >= requested_amount) {
            break;
        }
    }
    for (dbd::Id drop_id : empty_drops) {
        world.dropped_cargo.erase(drop_id);
    }
    (void)owner_player_id;
    return consumed;
}

void UpdatePlayerEconomicTelemetry(
    WorldState& world,
    dbd::Id player_id,
    dbd::PlayerState& player,
    bool spend_resources_and_credits) {
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
    player.credit_load = upkeep + tax;
    player.food_load = food;
    player.complexity_load = static_cast<float>(living_units) + (static_cast<float>(active_structures) * 0.5f);

    float food_available = 0.0f;
    if (spend_resources_and_credits) {
        for (auto& [_, storage] : world.storage_sites) {
            if (storage.owner_player_id != player_id) {
                continue;
            }
            food_available += ConsumeStoredResourceAmount(storage.stored_resources, food - food_available);
            if (food_available >= food) {
                break;
            }
        }
    } else {
        for (const auto& [_, storage] : world.storage_sites) {
            if (storage.owner_player_id != player_id) {
                continue;
            }
            for (const auto& stack : storage.stored_resources) {
                if (stack.category != dbd::CargoCategory::Resource || stack.amount == 0) {
                    continue;
                }
                food_available += static_cast<float>(stack.amount);
                if (food_available >= food) {
                    break;
                }
            }
            if (food_available >= food) {
                break;
            }
        }
    }

    player.food_shortage_ratio = food > 0.0f ? std::max(0.0f, food - food_available) / food : 0.0f;

    if (spend_resources_and_credits) {
        const float paid = std::min(player.credits, player.credit_load);
        player.credits -= paid;
        player.upkeep_shortage_ratio = player.credit_load > 0.0f
            ? std::max(0.0f, player.credit_load - paid) / player.credit_load
            : 0.0f;
    } else {
        player.upkeep_shortage_ratio = player.credit_load > 0.0f
            ? std::max(0.0f, player.credit_load - player.credits) / player.credit_load
            : 0.0f;
    }

    player.shortage_ratio = std::max(player.food_shortage_ratio, player.upkeep_shortage_ratio);
    const float labor_penalty = (player.food_shortage_ratio * 0.35f) + (player.upkeep_shortage_ratio * 0.30f);
    player.efficiency = std::max(0.45f, 1.0f - labor_penalty);
    player.pressure_ratio = 1.0f - player.efficiency;
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

void ClearQueuedOrders(dbd::UnitState& unit) {
    unit.queued_orders.clear();
    unit.queue_interrupt_reason.clear();
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

CommandResult SetUnitAutomationRules(
    WorldState& world,
    dbd::Id controller_player_id,
    dbd::Id unit_id,
    const std::vector<dbd::AutomationRule>& automation_rules) {
    const auto unit_it = world.units.find(unit_id);
    if (unit_it == world.units.end()) {
        return {false, "Unit not found."};
    }
    if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
        return {false, "Unit command authority denied for automation update."};
    }

    unit_it->second.automation_rules = automation_rules;
    return {true, "Unit automation updated.", unit_id};
}

CommandResult SetSquadAutomationRules(
    WorldState& world,
    dbd::Id owner_player_id,
    dbd::Id squad_id,
    const std::vector<dbd::AutomationRule>& automation_rules) {
    auto squad_it = world.squads.find(squad_id);
    if (squad_it == world.squads.end()) {
        return {false, "Squad not found."};
    }
    if (squad_it->second.owner_player_id != owner_player_id) {
        return {false, "Only the owning player can change squad automation."};
    }

    squad_it->second.automation_rules = automation_rules;
    return {true, "Squad automation updated.", squad_id};
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
    if (unit_ids.empty()) {
        return {false, "No units supplied for move order."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Controller does not own command authority for one or more units."};
        }
    }

    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.move_target = target;
        unit.assignment.kind = dbd::JobKind::None;
        unit.assignment.target_entity_id = 0;
        unit.assignment.target_position = target;
        GetOrCreateChunk(world, WorldToChunk(target)).opened_today = true;
    }

    return {true, "Move order accepted."};
}

CommandResult IssueFormationMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for formation_move."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Formation move denied for one or more units."};
        }
    }
    for (std::size_t i = 0; i < unit_ids.size(); ++i) {
        const dbd::Vec3 slot_target = FormationOffsetTarget(target, i, unit_ids.size());
        auto& unit = world.units.at(unit_ids[i]);
        RemoveUnitFromAssignedJobs(world, unit.unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.move_target = slot_target;
        unit.assignment.kind = dbd::JobKind::None;
        unit.assignment.target_entity_id = 0;
        unit.assignment.target_position = slot_target;
        GetOrCreateChunk(world, WorldToChunk(slot_target)).opened_today = true;
    }
    return {true, "Formation move order accepted.", unit_ids.front()};
}

CommandResult QueueMoveOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, const dbd::Vec3& target) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for queue_move."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Queue move denied for one or more units."};
        }
    }
    std::size_t replaced_count = 0;
    for (std::size_t i = 0; i < unit_ids.size(); ++i) {
        const dbd::Vec3 slot_target = FormationOffsetTarget(target, i, unit_ids.size());
        dbd::QueuedOrderState queued {};
        queued.order_type = dbd::UnitOrderType::Move;
        queued.move_target = slot_target;
        queued.assignment.kind = dbd::JobKind::None;
        queued.assignment.target_position = slot_target;
        auto& unit = world.units.at(unit_ids[i]);
        replaced_count += QueueOrder(unit, queued) ? 1 : 0;
        GetOrCreateChunk(world, WorldToChunk(slot_target)).opened_today = true;
    }
    std::ostringstream message;
    message << "queue_move accepted for " << unit_ids.size() << " unit(s)";
    if (replaced_count > 0) {
        message << "; queue full, oldest replaced for " << replaced_count << " unit(s)";
    }
    message << '.';
    return {true, message.str(), unit_ids.front()};
}

CommandResult ClearQueuedOrders(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for clear_queue."};
    }
    std::size_t cleared_units = 0;
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Clear queue denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        if (!unit.queued_orders.empty()) {
            ++cleared_units;
        }
        unit.queued_orders.clear();
    }
    std::ostringstream message;
    message << "queue cleared for " << cleared_units << " unit(s); active orders preserved.";
    return {true, message.str(), unit_ids.front()};
}

constexpr float kInteractionRange = 5.25f;

CommandResult CompleteHarvestInteraction(WorldState& world, dbd::Id unit_id, dbd::Id resource_node_id) {
    auto unit_it = world.units.find(unit_id);
    auto node_it = world.resource_nodes.find(resource_node_id);
    if (unit_it == world.units.end() || node_it == world.resource_nodes.end()) {
        return {false, "Harvest interaction target disappeared."};
    }

    auto& unit = unit_it->second;
    auto& node = node_it->second;
    if (Distance(unit.position, node.position) > kInteractionRange) {
        return {false, "Harvest target is still out of reach."};
    }
    if (node.remaining_amount == 0) {
        ClearUnitAssignment(unit);
        unit.current_order = dbd::UnitOrderType::Idle;
        unit.move_target = unit.position;
        return {false, "Resource node depleted."};
    }

    const float throughput = HarvestLaborPerSecond(unit) * EconomicLaborMultiplier(world, unit) * node.extraction_rate;
    const std::uint32_t amount = std::min<std::uint32_t>(static_cast<std::uint32_t>(std::ceil(throughput * 4.0f)), node.remaining_amount);
    if (amount == 0) {
        return {false, "No harvestable resource was produced."};
    }

    node.remaining_amount -= amount;
    unit.cargo.push_back(CreateHarvestCargoStack(node, amount));
    AwardHarvestXp(unit, static_cast<float>(amount));
    ClearUnitAssignment(unit);
    unit.current_order = dbd::UnitOrderType::Idle;
    unit.move_target = unit.position;
    unit.tactical_state = "Harvest completed";
    TryStartNextQueuedOrder(unit);
    return {true, "Harvest completed.", resource_node_id};
}

CommandResult CompleteLootInteraction(WorldState& world, dbd::Id unit_id, dbd::Id dropped_cargo_id) {
    auto unit_it = world.units.find(unit_id);
    auto drop_it = world.dropped_cargo.find(dropped_cargo_id);
    if (unit_it == world.units.end() || drop_it == world.dropped_cargo.end()) {
        return {false, "Dropped cargo interaction target disappeared."};
    }

    auto& unit = unit_it->second;
    auto& drop = drop_it->second;
    if (Distance(unit.position, drop.position) > kInteractionRange) {
        return {false, "Dropped cargo is still out of reach."};
    }

    const float looted_weight = CargoWeight(drop.cargo);
    unit.cargo.insert(unit.cargo.end(), drop.cargo.begin(), drop.cargo.end());
    drop.cargo.clear();
    world.dropped_cargo.erase(dropped_cargo_id);
    ClearUnitAssignment(unit);
    unit.current_order = dbd::UnitOrderType::Idle;
    unit.move_target = unit.position;
    unit.tactical_state = "Loot collected";

    unit.skill_progress.haul_xp += looted_weight * 0.08f;
    while (unit.skill_progress.haul_xp >= 12.0f) {
        unit.skill_progress.haul_xp -= 12.0f;
        ++unit.skills.haul_level;
    }
    TryStartNextQueuedOrder(unit);
    return {true, "Dropped cargo looted into unit cargo.", dropped_cargo_id};
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

    RemoveUnitFromAssignedJobs(world, unit_id);
    ClearQueuedOrders(unit);
    unit.current_order = dbd::UnitOrderType::Harvest;
    unit.assignment.kind = dbd::JobKind::Harvest;
    unit.assignment.target_entity_id = resource_node_id;
    unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
    unit.assignment.target_position = node.position;
    unit.move_target = node.position;
    unit.tactical_state = Distance(unit.position, node.position) <= kInteractionRange
        ? "Harvesting nearby resource"
        : "Approaching resource";
    if (Distance(unit.position, node.position) <= kInteractionRange) {
        return CompleteHarvestInteraction(world, unit_id, resource_node_id);
    }
    return {true, "Harvest target accepted; unit is approaching the resource.", resource_node_id};
}

CommandResult IssueLootOrder(WorldState& world, dbd::Id controller_player_id, dbd::Id unit_id, dbd::Id dropped_cargo_id) {
    if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueGather)) {
        return {false, "Unit command authority denied for loot."};
    }

    auto unit_it = world.units.find(unit_id);
    if (unit_it == world.units.end()) {
        return {false, "Unit not found."};
    }
    auto drop_it = world.dropped_cargo.find(dropped_cargo_id);
    if (drop_it == world.dropped_cargo.end()) {
        return {false, "Dropped cargo not found."};
    }

    auto& unit = unit_it->second;
    if (!unit.alive || unit.permanently_dead) {
        return {false, "Dead units cannot loot cargo."};
    }

    RemoveUnitFromAssignedJobs(world, unit_id);
    ClearQueuedOrders(unit);
    unit.current_order = dbd::UnitOrderType::Harvest;
    unit.assignment = {};
    unit.assignment.kind = dbd::JobKind::Haul;
    unit.assignment.route_source_kind = dbd::HaulRouteSourceKind::DroppedCargo;
    unit.assignment.target_entity_id = dropped_cargo_id;
    unit.assignment.route_source_id = dropped_cargo_id;
    unit.assignment.target_position = drop_it->second.position;
    unit.move_target = drop_it->second.position;
    unit.tactical_state = Distance(unit.position, drop_it->second.position) <= kInteractionRange
        ? "Collecting nearby loot"
        : "Approaching dropped cargo";
    if (Distance(unit.position, drop_it->second.position) <= kInteractionRange) {
        return CompleteLootInteraction(world, unit_id, dropped_cargo_id);
    }
    return {true, "Loot target accepted; unit is approaching dropped cargo.", dropped_cargo_id};
}

CommandResult IssueReturnToStorageOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    dbd::Id unit_id,
    dbd::Id storage_site_id) {
    if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
        return {false, "Unit command authority denied for return to storage."};
    }

    auto unit_it = world.units.find(unit_id);
    if (unit_it == world.units.end()) {
        return {false, "Unit not found."};
    }

    auto& unit = unit_it->second;
    if (!unit.alive || unit.permanently_dead) {
        return {false, "Dead units cannot return to storage."};
    }
    if (unit.cargo.empty()) {
        return {false, "Unit cargo is empty."};
    }

    if (storage_site_id == 0) {
        for (const auto& [candidate_id, storage] : world.storage_sites) {
            if (storage.owner_player_id == unit.owner_player_id) {
                storage_site_id = candidate_id;
                break;
            }
        }
    }

    auto storage_it = world.storage_sites.find(storage_site_id);
    if (storage_site_id == 0 || storage_it == world.storage_sites.end()) {
        return {false, "No storage site available for return_to_storage."};
    }
    if (storage_it->second.owner_player_id != unit.owner_player_id) {
        return {false, "Unit cannot return cargo to foreign storage."};
    }

    ClearQueuedOrders(unit);
    unit.current_order = dbd::UnitOrderType::HaulToStorage;
    RemoveUnitFromAssignedJobs(world, unit.unit_id);
    unit.assignment = {};
    unit.assignment.kind = dbd::JobKind::Haul;
    unit.assignment.target_entity_id = storage_site_id;
    unit.assignment.target_position = storage_it->second.position;
    unit.move_target = storage_it->second.position;
    return {true, "Unit redirected to storage.", storage_site_id};
}

CommandResult IssueEmergencyDepositOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for emergency_deposit."};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Emergency deposit denied for one or more units."};
        }
    }

    std::size_t redirected_units = 0;
    for (dbd::Id unit_id : unit_ids) {
        auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end()) {
            continue;
        }
        auto& unit = unit_it->second;
        if (!unit.alive || unit.permanently_dead || unit.cargo.empty()) {
            continue;
        }

        const auto storage_id = NearestOwnedStorageForUnit(world, unit);
        if (!storage_id.has_value()) {
            return {false, "No owned storage for emergency deposit."};
        }
        const auto storage_it = world.storage_sites.find(*storage_id);
        if (storage_it == world.storage_sites.end()) {
            return {false, "No owned storage for emergency deposit."};
        }

        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.assignment = {};
        unit.current_order = dbd::UnitOrderType::HaulToStorage;
        unit.assignment.kind = dbd::JobKind::Haul;
        unit.assignment.target_entity_id = *storage_id;
        unit.assignment.target_position = storage_it->second.position;
        unit.move_target = storage_it->second.position;
        unit.tactical_state = "Emergency deposit";
        ++redirected_units;
    }

    if (redirected_units == 0) {
        return {false, "No cargo-carrying units available for emergency deposit."};
    }

    std::ostringstream message;
    message << "Emergency deposit ordered for " << redirected_units << " unit(s).";
    return {true, message.str(), unit_ids.front()};
}

CommandResult IssueDropCargoOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for drop_cargo."};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Drop cargo denied for one or more units."};
        }
    }

    std::size_t dropped_units = 0;
    for (dbd::Id unit_id : unit_ids) {
        auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end()) {
            continue;
        }
        auto& unit = unit_it->second;
        if (!unit.alive || unit.permanently_dead || unit.cargo.empty()) {
            continue;
        }

        const auto dropped_weight = CargoWeight(unit.cargo);
        CreateDroppedCargo(world, unit_id, unit.position, unit.cargo);
        unit.cargo.clear();
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.assignment = {};
        unit.current_order = dbd::UnitOrderType::Idle;
        unit.move_target = unit.position;
        unit.tactical_state = "Cargo dropped";
        unit.skill_progress.haul_xp += dropped_weight * 0.02f;
        ++dropped_units;
    }

    if (dropped_units == 0) {
        return {false, "No cargo available to drop."};
    }

    std::ostringstream message;
    message << "Cargo dropped by " << dropped_units << " unit(s).";
    return {true, message.str(), unit_ids.front()};
}

CommandResult IssueGuardThreatenedRouteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for guard_threatened_route."};
    }
    RefreshRouteThreats(world);
    const auto hauler_id = FindBestThreatenedRouteHauler(world, controller_player_id, unit_ids);
    if (!hauler_id.has_value()) {
        return {false, "No threatened friendly route.", 0};
    }

    std::vector<dbd::Id> guard_unit_ids {};
    guard_unit_ids.reserve(unit_ids.size());
    for (dbd::Id unit_id : unit_ids) {
        if (unit_id != *hauler_id) {
            guard_unit_ids.push_back(unit_id);
        }
    }
    if (guard_unit_ids.empty()) {
        return {false, "No guard units supplied after excluding the threatened hauler.", *hauler_id};
    }

    auto result = IssueGuardUnitOrder(world, controller_player_id, guard_unit_ids, *hauler_id);
    if (result.ok) {
        result.message = "Guard threatened route order accepted.";
    }
    return result;
}

CommandResult IssueInterceptRouteThreatOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for intercept_route_threat."};
    }
    RefreshRouteThreats(world);
    const auto hauler_id = FindBestThreatenedRouteHauler(world, controller_player_id, unit_ids);
    if (!hauler_id.has_value()) {
        return {false, "No threatened friendly route.", 0};
    }

    const auto hauler_it = world.units.find(*hauler_id);
    if (hauler_it == world.units.end() || hauler_it->second.route_threat_enemy_id == 0) {
        return {false, "No route threat enemy available.", *hauler_id};
    }

    const dbd::Id threat_enemy_id = hauler_it->second.route_threat_enemy_id;
    const auto target_it = world.units.find(threat_enemy_id);
    if (target_it == world.units.end() || !target_it->second.alive || target_it->second.permanently_dead) {
        return {false, "Route threat enemy is no longer available.", threat_enemy_id};
    }
    if (IsFriendlyUnitTarget(world, controller_player_id, threat_enemy_id)) {
        return {false, "Route threat enemy is not hostile.", threat_enemy_id};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Intercept route threat denied for one or more units."};
        }
    }

    const auto& target = target_it->second;
    const float target_route_distance = Distance(target.position, target.move_target);
    if (target_route_distance <= 1.0f) {
        auto result = ResolveAttack(world, controller_player_id, unit_ids, dbd::AttackTargetKind::Unit, threat_enemy_id);
        if (result.ok) {
            result.message = "Intercept route threat order accepted.";
        }
        return result;
    }

    constexpr float lead_distance = 4.5f;
    const float inv_distance = 1.0f / std::max(target_route_distance, 0.0001f);
    const dbd::Vec3 intercept_point {
        target.position.x + ((target.move_target.x - target.position.x) * inv_distance * lead_distance),
        target.position.y + ((target.move_target.y - target.position.y) * inv_distance * lead_distance),
        target.position.z + ((target.move_target.z - target.position.z) * inv_distance * lead_distance)};

    for (dbd::Id unit_id : unit_ids) {
        auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
            continue;
        }
        auto& unit = unit_it->second;
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.assignment.kind = dbd::JobKind::Scout;
        unit.assignment.target_entity_id = threat_enemy_id;
        unit.assignment.target_position = intercept_point;
        unit.move_target = intercept_point;
        unit.tactical_state = "Intercepting route threat";
    }

    return {true, "Intercept route threat order accepted.", threat_enemy_id};
}

CommandResult IssueHaulRouteOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    dbd::HaulRouteSourceKind source_kind,
    dbd::Id source_id,
    dbd::Id storage_site_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for haul_route."};
    }
    if (source_kind == dbd::HaulRouteSourceKind::None || source_id == 0) {
        return {false, "Haul route source is required."};
    }
    const auto source_position = HaulSourcePosition(world, source_kind, source_id);
    if (!source_position.has_value()) {
        return {false, "Haul route source is missing or depleted."};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueGather)) {
            return {false, "Haul route denied for one or more units."};
        }
    }

    std::size_t routed_units = 0;
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }
        dbd::Id resolved_storage_id = storage_site_id;
        if (resolved_storage_id == 0) {
            const auto nearest_storage = NearestOwnedStorageForUnit(world, unit);
            if (!nearest_storage.has_value()) {
                return {false, "No owned storage available for haul_route."};
            }
            resolved_storage_id = *nearest_storage;
        }
        const auto storage_it = world.storage_sites.find(resolved_storage_id);
        if (storage_it == world.storage_sites.end() || storage_it->second.owner_player_id != unit.owner_player_id) {
            return {false, "Haul route storage missing or foreign."};
        }

        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::HaulToStorage;
        unit.assignment = {};
        unit.assignment.kind = dbd::JobKind::Haul;
        unit.assignment.route_active = true;
        unit.assignment.route_source_kind = source_kind;
        unit.assignment.route_phase = unit.cargo.empty() ? dbd::HaulRoutePhase::ToSource : dbd::HaulRoutePhase::ToStorage;
        unit.assignment.route_source_id = source_id;
        unit.assignment.route_storage_id = resolved_storage_id;
        unit.assignment.target_entity_id = unit.assignment.route_phase == dbd::HaulRoutePhase::ToSource ? source_id : resolved_storage_id;
        unit.assignment.target_position = unit.assignment.route_phase == dbd::HaulRoutePhase::ToSource ? *source_position : storage_it->second.position;
        unit.move_target = unit.assignment.target_position;
        unit.tactical_state = "Haul route active";
        GetOrCreateChunk(world, WorldToChunk(unit.move_target)).opened_today = true;
        ++routed_units;
    }

    std::ostringstream message;
    message << "haul_route accepted for " << routed_units << " unit(s).";
    return {routed_units > 0, message.str(), source_id};
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
    if (!unit.alive || unit.permanently_dead) {
        return {false, "Unit is not able to deliver cargo."};
    }
    if (unit.current_order != dbd::UnitOrderType::HaulToStorage ||
        unit.assignment.kind != dbd::JobKind::Haul ||
        unit.assignment.target_entity_id != storage_site_id) {
        return {false, "Unit is not assigned to haul cargo to this storage site."};
    }
    if (storage.owner_player_id != unit.owner_player_id) {
        return {false, "Unit cannot deliver cargo to storage it does not own."};
    }
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

CommandResult ProgressHaulRoutes(WorldState& world, float delta_seconds) {
    (void)delta_seconds;
    std::size_t active_routes = 0;
    std::size_t loads = 0;
    std::size_t deliveries = 0;
    std::size_t interrupted = 0;

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || !unit.assignment.route_active) {
            continue;
        }
        ++active_routes;
        auto storage_it = world.storage_sites.find(unit.assignment.route_storage_id);
        if (storage_it == world.storage_sites.end() || storage_it->second.owner_player_id != unit.owner_player_id) {
            unit.assignment.route_phase = dbd::HaulRoutePhase::Interrupted;
            unit.assignment.route_active = false;
            unit.tactical_state = "Route interrupted: storage missing";
            ++interrupted;
            continue;
        }

        if (unit.assignment.route_phase == dbd::HaulRoutePhase::ToSource &&
            Distance(unit.position, unit.assignment.target_position) <= 5.25f) {
            unit.assignment.route_phase = dbd::HaulRoutePhase::Loading;
            bool loaded = false;
            if (unit.assignment.route_source_kind == dbd::HaulRouteSourceKind::ResourceNode) {
                auto node_it = world.resource_nodes.find(unit.assignment.route_source_id);
                if (node_it != world.resource_nodes.end() && node_it->second.remaining_amount > 0) {
                    auto& node = node_it->second;
                    const float throughput = HarvestLaborPerSecond(unit) * EconomicLaborMultiplier(world, unit) * node.extraction_rate;
                    const std::uint32_t amount = std::min<std::uint32_t>(
                        static_cast<std::uint32_t>(std::ceil(throughput * 4.0f)),
                        node.remaining_amount);
                    if (amount > 0) {
                        node.remaining_amount -= amount;
                        unit.cargo.push_back(CreateHarvestCargoStack(node, amount));
                        AwardHarvestXp(unit, static_cast<float>(amount));
                        loaded = true;
                    }
                }
            } else if (unit.assignment.route_source_kind == dbd::HaulRouteSourceKind::DroppedCargo) {
                auto drop_it = world.dropped_cargo.find(unit.assignment.route_source_id);
                if (drop_it != world.dropped_cargo.end() && !drop_it->second.cargo.empty()) {
                    const float looted_weight = CargoWeight(drop_it->second.cargo);
                    unit.cargo.insert(unit.cargo.end(), drop_it->second.cargo.begin(), drop_it->second.cargo.end());
                    drop_it->second.cargo.clear();
                    world.dropped_cargo.erase(drop_it);
                    unit.skill_progress.haul_xp += looted_weight * 0.08f;
                    while (unit.skill_progress.haul_xp >= 12.0f) {
                        unit.skill_progress.haul_xp -= 12.0f;
                        ++unit.skills.haul_level;
                    }
                    loaded = true;
                }
            }

            if (!loaded) {
                unit.assignment.route_phase = dbd::HaulRoutePhase::Interrupted;
                unit.assignment.route_active = false;
                unit.tactical_state = "Route interrupted: source empty";
                ++interrupted;
                continue;
            }
            unit.assignment.route_phase = dbd::HaulRoutePhase::ToStorage;
            unit.assignment.target_entity_id = unit.assignment.route_storage_id;
            unit.assignment.target_position = storage_it->second.position;
            unit.move_target = storage_it->second.position;
            unit.tactical_state = CargoAtOrOverCapacity(unit) ? "Haul route loaded: overloaded" : "Haul route loaded";
            ++loads;
        } else if (unit.assignment.route_phase == dbd::HaulRoutePhase::ToStorage &&
                   Distance(unit.position, storage_it->second.position) <= 5.25f) {
            unit.assignment.route_phase = dbd::HaulRoutePhase::Unloading;
            const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.route_storage_id);
            if (delivery.ok) {
                ++deliveries;
            }
            const auto next_source_position = HaulSourcePosition(world, unit.assignment.route_source_kind, unit.assignment.route_source_id);
            if (!next_source_position.has_value()) {
                unit.assignment.route_phase = dbd::HaulRoutePhase::Interrupted;
                unit.assignment.route_active = false;
                unit.tactical_state = "Route complete: source empty";
                ++interrupted;
                continue;
            }
            unit.assignment.route_phase = dbd::HaulRoutePhase::ToSource;
            unit.assignment.target_entity_id = unit.assignment.route_source_id;
            unit.assignment.target_position = *next_source_position;
            unit.move_target = *next_source_position;
            unit.tactical_state = "Haul route returning to source";
        }
    }

    std::ostringstream message;
    message << "Haul routes progressed for " << active_routes << " unit(s), "
            << loads << " load(s), " << deliveries << " delivery(ies), "
            << interrupted << " interruption(s).";
    return {true, message.str()};
}

CommandResult RefreshRouteThreats(WorldState& world) {
    // Route-threat refresh can be invoked by regression/bootstrap code after direct
    // position edits, so rebuild the read index before issuing localized queries.
    MarkUnitSpatialIndexDirty(world);
    RefreshUnitSpatialIndex(world);

    std::size_t active_routes = 0;
    std::size_t threatened_routes = 0;
    std::size_t critical_routes = 0;

    for (auto& [_, unit] : world.units) {
        unit.route_threat_level = "safe";
        unit.route_threat_enemy_id = 0;
    }

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || !unit.assignment.route_active) {
            continue;
        }
        ++active_routes;

        const bool overloaded = CargoWeight(unit.cargo) > std::max(1.0f, unit.carry_capacity);
        const auto source_position = HaulSourcePosition(world, unit.assignment.route_source_kind, unit.assignment.route_source_id);
        std::optional<dbd::Vec3> storage_position;
        const auto storage_it = world.storage_sites.find(unit.assignment.route_storage_id);
        if (storage_it != world.storage_sites.end()) {
            storage_position = storage_it->second.position;
        }

        std::vector<dbd::Id> route_nearby_ids;
        const auto collect_nearby = [&](const dbd::Vec3& center, float radius) {
            for (dbd::Id candidate_id : FindNearbyUnitIds(world, center, radius)) {
                if (!ContainsId(route_nearby_ids, candidate_id)) {
                    route_nearby_ids.push_back(candidate_id);
                }
            }
        };
        collect_nearby(unit.position, overloaded ? 26.0f : 19.0f);
        if (source_position.has_value()) {
            collect_nearby(*source_position, 13.0f);
        }
        if (storage_position.has_value()) {
            collect_nearby(*storage_position, 15.0f);
        }
        std::sort(route_nearby_ids.begin(), route_nearby_ids.end(), [&](dbd::Id left_id, dbd::Id right_id) {
            const auto left_it = world.units.find(left_id);
            const auto right_it = world.units.find(right_id);
            const float left_distance =
                left_it == world.units.end() ? std::numeric_limits<float>::max() : DistanceSquared(unit.position, left_it->second.position);
            const float right_distance =
                right_it == world.units.end() ? std::numeric_limits<float>::max() : DistanceSquared(unit.position, right_it->second.position);
            return left_distance < right_distance;
        });

        for (dbd::Id enemy_id : route_nearby_ids) {
            const auto enemy_it = world.units.find(enemy_id);
            if (enemy_it == world.units.end()) {
                continue;
            }
            const auto& enemy = enemy_it->second;
            if (!enemy.alive || enemy.permanently_dead || enemy.owner_player_id == unit.owner_player_id) {
                continue;
            }

            const float distance_to_hauler = Distance(unit.position, enemy.position);
            if (distance_to_hauler <= 7.0f || (overloaded && distance_to_hauler <= 13.0f)) {
                RaiseRouteThreat(unit, "critical", enemy_id);
            } else if (distance_to_hauler <= 19.0f || (overloaded && distance_to_hauler <= 26.0f)) {
                RaiseRouteThreat(unit, "threatened", enemy_id);
            }

            if (source_position.has_value()) {
                const float distance_to_source = Distance(*source_position, enemy.position);
                if (distance_to_source <= 6.0f) {
                    RaiseRouteThreat(unit, "critical", enemy_id);
                } else if (distance_to_source <= 13.0f) {
                    RaiseRouteThreat(unit, "threatened", enemy_id);
                }
            }

            if (storage_position.has_value()) {
                const float distance_to_storage = Distance(*storage_position, enemy.position);
                if (distance_to_storage <= 7.0f) {
                    RaiseRouteThreat(unit, "critical", enemy_id);
                } else if (distance_to_storage <= 15.0f) {
                    RaiseRouteThreat(unit, "threatened", enemy_id);
                }
            }
        }

        if (unit.route_threat_level == "critical") {
            ++critical_routes;
            unit.tactical_state = "Route critical";
        } else if (unit.route_threat_level == "threatened") {
            ++threatened_routes;
            unit.tactical_state = "Route threatened";
        }
    }

    std::ostringstream message;
    message << "Route threats refreshed for " << active_routes << " active route(s), "
            << threatened_routes << " threatened, " << critical_routes << " critical.";
    return {true, message.str()};
}

CommandResult StartFlattenJob(WorldState& world, dbd::Id player_id, const dbd::Vec3& center, float radius, float target_grade) {
    if (!PlayerExists(world, player_id)) {
        return {false, "Flatten job rejected: player not found."};
    }

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

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "One or more assigned units are not controllable."};
        }
    }

    job.state = dbd::FlattenJobStateKind::InProgress;

    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
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
    if (!PlayerExists(world, owner_player_id)) {
        return {false, "Construction rejected: owning player not found."};
    }

    const float allowed_slope = AllowedSlopeForStructure(structure_type);
    const auto terrain = EvaluateTerrainAt(world, position, footprint_radius, allowed_slope);
    const auto time_class = BuildTimeClassFor(structure_type, footprint_radius);

    if (!terrain.already_buildable) {
        std::ostringstream message;
        message << "Construction cannot start until the footprint is flattened; slope "
                << terrain.average_slope << " exceeds allowed " << allowed_slope << '.';
        return {false, message.str()};
    }

    if (ConstructionFootprintOverlapsWorld(world, position, footprint_radius)) {
        return {false, "Construction footprint overlaps an existing object."};
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
        false,
        0);

    auto& mutable_site = world.construction_sites.at(site.construction_site_id);
    mutable_site.stage = dbd::ConstructionStage::UnderConstruction;
    mutable_site.completion_health_ratio = 0.0f;

    return {true, "Construction site created and auto-building started.", site.construction_site_id};
}

CommandResult AssignUnitsToConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids) {
    (void)world;
    (void)controller_player_id;
    (void)construction_site_id;
    (void)unit_ids;
    return {false, "Construction is automatic in this server slice; use units for flattening, escort, or defense instead."};
}

CommandResult RemoveUnitsFromConstruction(WorldState& world, dbd::Id controller_player_id, dbd::Id construction_site_id, const std::vector<dbd::Id>& unit_ids) {
    (void)world;
    (void)controller_player_id;
    (void)construction_site_id;
    (void)unit_ids;
    return {false, "Construction sites no longer use assigned builder units."};
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

        if (site.stage == dbd::ConstructionStage::WaitingForFlatten) {
            ++it;
            continue;
        }

        site.stage = dbd::ConstructionStage::UnderConstruction;
        const float labor = AutoConstructionProgressPerSecond(site) * delta_seconds;
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
    ClearQueuedOrders(unit);

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
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::AttackTarget;
        unit.assignment.kind = dbd::JobKind::Attack;
        unit.assignment.target_entity_id = target_entity_id;
        unit.assignment.target_kind = target_kind;
        unit.assignment.target_position = target_position;
        unit.move_target = target_position;
    }
    return {true, "Attack order registered.", target_entity_id};
}

CommandResult IssueRetreatOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for retreat_selected."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Retreat denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Retreat;
        unit.assignment.kind = dbd::JobKind::Retreat;
        unit.assignment.target_position = ComputeRetreatTarget(world, unit);
        unit.move_target = unit.assignment.target_position;
    }
    return {true, "Retreat selected order accepted.", unit_ids.front()};
}

CommandResult IssueFocusFireOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& attacker_unit_ids,
    dbd::AttackTargetKind target_kind,
    dbd::Id target_entity_id) {
    if (attacker_unit_ids.empty()) {
        return {false, "No attackers supplied for focus_fire."};
    }
    for (dbd::Id unit_id : attacker_unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueAttack)) {
            return {false, "Focus fire denied for one or more units."};
        }
    }

    RefreshKnownContacts(world);
    if (!HasFreshContact(world, controller_player_id, target_kind, target_entity_id)) {
        const auto contact = FindKnownContact(world, controller_player_id, target_kind, target_entity_id);
        if (contact.has_value()) {
            MoveUnitsToLastSeenContact(world, controller_player_id, attacker_unit_ids, *contact);
            return {true, "Focus target is stale; moving to last seen contact.", target_entity_id};
        }
        return {false, "Focus fire denied: target has not been scouted."};
    }

    auto result = ResolveAttack(world, controller_player_id, attacker_unit_ids, target_kind, target_entity_id);
    if (result.ok) {
        for (dbd::Id unit_id : attacker_unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it != world.units.end()) {
                unit_it->second.assignment.delegated = true;
            }
        }
        result.message = "Focus fire order registered.";
    }
    return result;
}

CommandResult IssueInterceptUnitOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id target_unit_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for intercept_unit."};
    }
    RefreshKnownContacts(world);
    const auto target_it = world.units.find(target_unit_id);
    if (target_it == world.units.end() || !target_it->second.alive || target_it->second.permanently_dead) {
        return {false, "Intercept target unit not found or unavailable."};
    }
    if (IsFriendlyUnitTarget(world, controller_player_id, target_unit_id)) {
        return {false, "Intercept target must be hostile."};
    }
    if (!HasFreshContact(world, controller_player_id, dbd::AttackTargetKind::Unit, target_unit_id)) {
        const auto contact = FindKnownContact(world, controller_player_id, dbd::AttackTargetKind::Unit, target_unit_id);
        if (contact.has_value()) {
            MoveUnitsToLastSeenContact(world, controller_player_id, unit_ids, *contact);
            return {true, "Intercept target is stale; moving to last seen contact.", target_unit_id};
        }
        return {false, "Intercept denied: target has not been scouted."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Intercept denied for one or more units."};
        }
    }

    const auto& target = target_it->second;
    const float target_route_distance = Distance(target.position, target.move_target);
    if (target_route_distance <= 1.0f) {
        auto result = ResolveAttack(world, controller_player_id, unit_ids, dbd::AttackTargetKind::Unit, target_unit_id);
        if (result.ok) {
            for (dbd::Id unit_id : unit_ids) {
                auto unit_it = world.units.find(unit_id);
                if (unit_it != world.units.end()) {
                    unit_it->second.assignment.delegated = true;
                }
            }
            result.message = "Intercept fallback registered as focus attack.";
        }
        return result;
    }

    const float lead_distance = std::min(8.0f, target_route_distance * 0.55f);
    const float inv_distance = 1.0f / std::max(target_route_distance, 0.0001f);
    const dbd::Vec3 intercept_point {
        target.position.x + ((target.move_target.x - target.position.x) * inv_distance * lead_distance),
        target.position.y + ((target.move_target.y - target.position.y) * inv_distance * lead_distance),
        target.position.z + ((target.move_target.z - target.position.z) * inv_distance * lead_distance)};

    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.assignment.kind = dbd::JobKind::Scout;
        unit.assignment.target_entity_id = target_unit_id;
        unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
        unit.assignment.target_position = intercept_point;
        unit.assignment.delegated = true;
        unit.move_target = intercept_point;
        GetOrCreateChunk(world, WorldToChunk(intercept_point)).opened_today = true;
    }

    return {true, "Intercept route order accepted.", target_unit_id};
}

CommandResult IssueScoutAreaOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::Vec3& center,
    float radius) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for scout_area."};
    }
    const float scout_radius = std::clamp(radius, 4.0f, 48.0f);
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Scout area denied for one or more units."};
        }
    }
    for (std::size_t i = 0; i < unit_ids.size(); ++i) {
        auto& unit = world.units.at(unit_ids[i]);
        RemoveUnitFromAssignedJobs(world, unit.unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        const float offset = (i % 2 == 0) ? scout_radius : -scout_radius;
        unit.current_order = dbd::UnitOrderType::Scout;
        unit.assignment.kind = dbd::JobKind::Scout;
        unit.assignment.target_position = center;
        unit.assignment.secondary_position = {center.x + offset, center.y, center.z + (offset * 0.5f)};
        unit.assignment.radius = scout_radius;
        unit.assignment.patrol_route = false;
        unit.assignment.patrol_to_secondary = false;
        unit.assignment.delegated = true;
        unit.move_target = center;
        GetOrCreateChunk(world, WorldToChunk(center)).opened_today = true;
    }
    return {true, "Scout area order accepted.", unit_ids.front()};
}

CommandResult QueueScoutAreaOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::Vec3& center,
    float radius) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for queue_scout_area."};
    }
    const float scout_radius = std::clamp(radius, 4.0f, 48.0f);
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Queued scout area denied for one or more units."};
        }
    }
    std::size_t replaced_count = 0;
    for (std::size_t i = 0; i < unit_ids.size(); ++i) {
        auto& unit = world.units.at(unit_ids[i]);
        const float offset = (i % 2 == 0) ? scout_radius : -scout_radius;
        dbd::QueuedOrderState queued {};
        queued.order_type = dbd::UnitOrderType::Scout;
        queued.move_target = center;
        queued.assignment.kind = dbd::JobKind::Scout;
        queued.assignment.target_position = center;
        queued.assignment.secondary_position = {center.x + offset, center.y, center.z + (offset * 0.5f)};
        queued.assignment.radius = scout_radius;
        queued.assignment.patrol_route = false;
        queued.assignment.patrol_to_secondary = false;
        queued.assignment.delegated = true;
        replaced_count += QueueOrder(unit, queued) ? 1 : 0;
        GetOrCreateChunk(world, WorldToChunk(center)).opened_today = true;
    }
    std::ostringstream message;
    message << "queue_scout_area accepted for " << unit_ids.size() << " unit(s)";
    if (replaced_count > 0) {
        message << "; queue full, oldest replaced for " << replaced_count << " unit(s)";
    }
    message << '.';
    return {true, message.str(), unit_ids.front()};
}

CommandResult IssuePatrolRouteOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::Vec3& point_a,
    const dbd::Vec3& point_b) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for patrol_route."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Patrol route denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Scout;
        unit.assignment.kind = dbd::JobKind::Scout;
        unit.assignment.target_position = point_a;
        unit.assignment.secondary_position = point_b;
        unit.assignment.radius = 0.0f;
        unit.assignment.patrol_route = true;
        unit.assignment.patrol_to_secondary = false;
        unit.assignment.delegated = true;
        unit.move_target = point_a;
        GetOrCreateChunk(world, WorldToChunk(point_a)).opened_today = true;
        GetOrCreateChunk(world, WorldToChunk(point_b)).opened_today = true;
    }
    return {true, "Patrol route order accepted.", unit_ids.front()};
}

CommandResult QueuePatrolRouteOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::Vec3& point_a,
    const dbd::Vec3& point_b) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for queue_patrol_route."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Queued patrol route denied for one or more units."};
        }
    }
    std::size_t replaced_count = 0;
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        dbd::QueuedOrderState queued {};
        queued.order_type = dbd::UnitOrderType::Scout;
        queued.move_target = point_a;
        queued.assignment.kind = dbd::JobKind::Scout;
        queued.assignment.target_position = point_a;
        queued.assignment.secondary_position = point_b;
        queued.assignment.radius = 0.0f;
        queued.assignment.patrol_route = true;
        queued.assignment.patrol_to_secondary = false;
        queued.assignment.delegated = true;
        replaced_count += QueueOrder(unit, queued) ? 1 : 0;
        GetOrCreateChunk(world, WorldToChunk(point_a)).opened_today = true;
        GetOrCreateChunk(world, WorldToChunk(point_b)).opened_today = true;
    }
    std::ostringstream message;
    message << "queue_patrol_route accepted for " << unit_ids.size() << " unit(s)";
    if (replaced_count > 0) {
        message << "; queue full, oldest replaced for " << replaced_count << " unit(s)";
    }
    message << '.';
    return {true, message.str(), unit_ids.front()};
}

CommandResult IssueInvestigateContactOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    dbd::AttackTargetKind target_kind,
    dbd::Id target_entity_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for investigate_contact."};
    }
    RefreshKnownContacts(world);
    const auto contact = FindKnownContact(world, controller_player_id, target_kind, target_entity_id);
    if (!contact.has_value()) {
        return {false, "Investigate contact rejected: contact expired or has not been scouted.", target_entity_id};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Investigate contact denied for one or more units.", target_entity_id};
        }
    }
    MoveUnitsToLastSeenContact(world, controller_player_id, unit_ids, *contact);
    if (contact->currently_visible || world.tick <= contact->last_seen_tick + kFreshContactTicks) {
        return {true, "Investigating recent contact.", target_entity_id};
    }
    return {true, "Investigating stale last-seen contact.", target_entity_id};
}

CommandResult IssueGuardUnitOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& guard_unit_ids,
    dbd::Id target_unit_id) {
    if (guard_unit_ids.empty()) {
        return {false, "No guard units supplied."};
    }
    const auto target_it = world.units.find(target_unit_id);
    if (target_it == world.units.end() || !target_it->second.alive || target_it->second.permanently_dead) {
        return {false, "Guard target unit not found or unavailable."};
    }
    if (target_it->second.owner_player_id != controller_player_id && target_it->second.controller_player_id != controller_player_id) {
        return {false, "Guard unit target must be friendly or controlled."};
    }

    for (dbd::Id unit_id : guard_unit_ids) {
        if (unit_id == target_unit_id || !HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Guard denied for one or more units."};
        }
    }

    for (dbd::Id unit_id : guard_unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Escort;
        unit.assignment.kind = dbd::JobKind::Escort;
        unit.assignment.target_entity_id = target_unit_id;
        unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
        unit.assignment.target_position = target_it->second.position;
        unit.move_target = target_it->second.position;
    }

    return {true, "Guard unit order accepted.", target_unit_id};
}

CommandResult IssueGuardSiteOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& guard_unit_ids,
    dbd::AttackTargetKind target_kind,
    dbd::Id target_entity_id) {
    if (guard_unit_ids.empty()) {
        return {false, "No guard units supplied."};
    }

    dbd::Vec3 anchor {};
    bool valid_target = false;
    if (target_kind == dbd::AttackTargetKind::ConstructionSite) {
        const auto site_it = world.construction_sites.find(target_entity_id);
        valid_target = site_it != world.construction_sites.end() && site_it->second.stage != dbd::ConstructionStage::Destroyed;
        if (valid_target) {
            anchor = site_it->second.position;
        }
    } else if (target_kind == dbd::AttackTargetKind::Structure) {
        const auto structure_it = world.structures.find(target_entity_id);
        valid_target = structure_it != world.structures.end();
        if (valid_target) {
            anchor = structure_it->second.position;
        }
    }
    if (!valid_target) {
        return {false, "Guard site target not found."};
    }

    for (dbd::Id unit_id : guard_unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Guard denied for one or more units."};
        }
    }

    for (dbd::Id unit_id : guard_unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Escort;
        unit.assignment.kind = dbd::JobKind::Escort;
        unit.assignment.target_entity_id = target_entity_id;
        unit.assignment.target_kind = target_kind;
        unit.assignment.target_position = anchor;
        unit.move_target = anchor;
    }

    return {true, "Guard site order accepted.", target_entity_id};
}

CommandResult IssueRepairStructureOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id structure_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for repair_structure."};
    }
    auto structure_it = world.structures.find(structure_id);
    if (structure_it == world.structures.end()) {
        return {false, "Repair target structure not found."};
    }
    auto& structure = structure_it->second;
    if (structure.owner_player_id != controller_player_id) {
        return {false, "Cannot repair foreign structure."};
    }
    if (structure.health >= structure.max_health) {
        return {false, "Structure does not need repair.", structure_id};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "Repair denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::BuildSite;
        unit.assignment.kind = dbd::JobKind::Build;
        unit.assignment.target_kind = dbd::AttackTargetKind::Structure;
        unit.assignment.target_entity_id = structure_id;
        unit.assignment.target_position = structure.position;
        unit.move_target = structure.position;
        unit.tactical_state = "Repairing structure";
    }
    return {true, "Repair structure order accepted.", structure_id};
}

CommandResult IssueRepairConstructionSiteOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id construction_site_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for repair_construction_site."};
    }
    auto site_it = world.construction_sites.find(construction_site_id);
    if (site_it == world.construction_sites.end()) {
        return {false, "Repair target construction site not found."};
    }
    auto& site = site_it->second;
    if (site.owner_player_id != controller_player_id) {
        return {false, "Cannot repair foreign construction site."};
    }
    if (site.stage == dbd::ConstructionStage::Destroyed || site.stage == dbd::ConstructionStage::Canceled ||
        site.stage == dbd::ConstructionStage::Completed) {
        return {false, "Construction site cannot be repaired in this state.", construction_site_id};
    }
    if (site.health >= site.max_health) {
        return {false, "Construction site does not need repair.", construction_site_id};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueBuild)) {
            return {false, "Repair denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::BuildSite;
        unit.assignment.kind = dbd::JobKind::Build;
        unit.assignment.target_kind = dbd::AttackTargetKind::ConstructionSite;
        unit.assignment.target_entity_id = construction_site_id;
        unit.assignment.target_position = site.position;
        unit.move_target = site.position;
        unit.tactical_state = "Repairing site";
    }
    return {true, "Repair construction site order accepted.", construction_site_id};
}

CommandResult IssueSupplyRepairOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::AttackTargetKind target_kind, dbd::Id target_entity_id) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for supply_repair."};
    }

    dbd::Vec3 target_position {};
    dbd::Id owner_player_id = 0;
    bool valid_target = false;
    if (target_kind == dbd::AttackTargetKind::Structure) {
        auto structure_it = world.structures.find(target_entity_id);
        if (structure_it == world.structures.end()) {
            return {false, "Repair supply target structure not found."};
        }
        if (structure_it->second.owner_player_id != controller_player_id) {
            return {false, "Cannot supply repair for foreign structure."};
        }
        if (structure_it->second.health >= structure_it->second.max_health) {
            return {false, "Structure does not need repair supply.", target_entity_id};
        }
        target_position = structure_it->second.position;
        owner_player_id = structure_it->second.owner_player_id;
        valid_target = true;
    } else if (target_kind == dbd::AttackTargetKind::ConstructionSite) {
        auto site_it = world.construction_sites.find(target_entity_id);
        if (site_it == world.construction_sites.end()) {
            return {false, "Repair supply target construction site not found."};
        }
        if (site_it->second.owner_player_id != controller_player_id) {
            return {false, "Cannot supply repair for foreign construction site."};
        }
        if (site_it->second.stage == dbd::ConstructionStage::Destroyed ||
            site_it->second.stage == dbd::ConstructionStage::Canceled ||
            site_it->second.stage == dbd::ConstructionStage::Completed) {
            return {false, "Construction site cannot receive repair supply in this state.", target_entity_id};
        }
        if (site_it->second.health >= site_it->second.max_health) {
            return {false, "Construction site does not need repair supply.", target_entity_id};
        }
        target_position = site_it->second.position;
        owner_player_id = site_it->second.owner_player_id;
        valid_target = true;
    }
    if (!valid_target) {
        return {false, "Repair supply requires a structure or construction site target."};
    }

    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Repair supply denied for one or more units."};
        }
    }

    constexpr float kRepairSupplyAmount = 24.0f;
    const float supplied = ConsumeOwnedRepairMaterials(world, owner_player_id, kRepairSupplyAmount);
    if (supplied <= 0.0f) {
        return {false, "No stored materials available for repair supply.", target_entity_id};
    }

    std::vector<dbd::CargoStack> repair_supply {
        dbd::CargoStack {target_entity_id, static_cast<std::uint32_t>(std::max(1.0f, std::floor(supplied))), supplied * 10.0f, 1.0f, dbd::CargoCategory::Resource}
    };
    dbd::Vec3 supply_position {target_position.x + 2.0f, target_position.y, target_position.z + 2.0f};
    auto& drop = CreateDroppedCargo(world, 0, supply_position, repair_supply);

    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Move;
        unit.assignment.kind = dbd::JobKind::Haul;
        unit.assignment.target_kind = target_kind;
        unit.assignment.target_entity_id = target_entity_id;
        unit.assignment.target_position = supply_position;
        unit.move_target = supply_position;
        unit.tactical_state = "Supplying repair";
    }

    return {true, "Repair supply staged near target.", drop.dropped_cargo_id};
}

std::vector<std::pair<dbd::Id, std::uint32_t>> CraftingRecipeFor(dbd::Id item_id) {
    switch (item_id) {
        case 92'001: return {{91'001, 4}, {91'002, 1}};              // Field Shovel
        case 92'002: return {{91'001, 6}, {91'002, 3}, {91'003, 2}}; // Flattening Tool
        case 92'003: return {{91'001, 8}, {91'003, 3}};              // Builder Kit
        case 92'004: return {{91'001, 1}};                           // Survey Marker
        case 92'005: return {{91'001, 10}, {91'003, 2}};             // Storage Crate
        case 92'006: return {{91'001, 3}, {91'003, 1}};              // Field Hammer
        default: return {};
    }
}

CommandResult CraftItem(WorldState& world, dbd::Id player_id, dbd::Id item_id, std::uint32_t amount) {
    if (!PlayerExists(world, player_id)) {
        return {false, "Craft rejected: player not found."};
    }
    const auto* definition = dbd::FindItemDefinition(item_id);
    if (definition == nullptr) {
        return {false, "Craft rejected: unknown item."};
    }
    if (amount == 0) {
        amount = 1;
    }
    const auto recipe = CraftingRecipeFor(item_id);
    if (recipe.empty()) {
        return {false, "Craft rejected: no simple recipe for this item."};
    }
    const auto storage_id = FindNearestStorageSite(world, player_id, dbd::Vec3 {});
    if (!storage_id.has_value()) {
        return {false, "Craft rejected: no owned storage."};
    }

    for (const auto& [ingredient_id, ingredient_amount] : recipe) {
        const float needed = static_cast<float>(ingredient_amount * amount);
        if (StoredItemAmountForPlayer(world, player_id, ingredient_id) < needed) {
            const auto* ingredient = dbd::FindItemDefinition(ingredient_id);
            return {false, std::string("Craft rejected: missing ") + (ingredient != nullptr ? ingredient->display_name : "ingredient") + "."};
        }
    }

    for (const auto& [ingredient_id, ingredient_amount] : recipe) {
        ConsumeStoredItemAmount(world, player_id, ingredient_id, static_cast<float>(ingredient_amount * amount));
    }
    AddStoredItem(world, player_id, item_id, amount);

    std::ostringstream message;
    message << "Crafted " << amount << " " << definition->display_name << ".";
    return {true, message.str(), item_id};
}

CommandResult InstallItem(WorldState& world, dbd::Id controller_player_id, dbd::Id item_id, const dbd::Vec3& position) {
    if (!PlayerExists(world, controller_player_id)) {
        return {false, "Install rejected: player not found."};
    }
    const auto* definition = dbd::FindItemDefinition(item_id);
    if (definition == nullptr) {
        return {false, "Install rejected: unknown item."};
    }
    if (StoredItemAmountForPlayer(world, controller_player_id, item_id) < 1.0f) {
        return {false, "Install rejected: item is not in owned storage."};
    }

    if (item_id == 92'005) {
        constexpr float kStorageCrateFootprint = 4.5f;
        constexpr float kStorageCrateAllowedSlope = 0.18f;
        const auto terrain = EvaluateTerrainAt(world, position, kStorageCrateFootprint, kStorageCrateAllowedSlope);
        if (!terrain.already_buildable) {
            std::ostringstream message;
            message << "Install rejected: storage crate needs flatter ground; slope "
                    << terrain.average_slope << " exceeds allowed " << kStorageCrateAllowedSlope << '.';
            return {false, message.str()};
        }
        if (ConstructionFootprintOverlapsWorld(world, position, kStorageCrateFootprint)) {
            return {false, "Install rejected: storage crate overlaps an existing object."};
        }
        ConsumeStoredItemAmount(world, controller_player_id, item_id, 1.0f);
        auto& structure = CreateStructure(
            world,
            controller_player_id,
            dbd::StructureType::StorageDepot,
            dbd::BuildTimeClass::SmallFast,
            position,
            kStorageCrateFootprint,
            kStorageCrateAllowedSlope);
        CreateStorageSite(world, structure.structure_id, controller_player_id, position);
        return {true, "Installed storage crate as a small depot.", structure.structure_id};
    }

    if (item_id == 92'004) {
        ConsumeStoredItemAmount(world, controller_player_id, item_id, 1.0f);
        std::vector<dbd::CargoStack> marker {
            dbd::CargoStack {item_id, 1, definition->base_value, definition->unit_weight, dbd::CargoCategory::Resource}
        };
        auto& drop = CreateDroppedCargo(world, 0, position, marker);
        return {true, "Installed survey marker.", drop.dropped_cargo_id};
    }

    return {false, "Install rejected: this item is not installable in v1."};
}

CommandResult UseItem(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids, dbd::Id item_id) {
    if (unit_ids.empty()) {
        return {false, "Use item rejected: no units selected."};
    }
    const auto* definition = dbd::FindItemDefinition(item_id);
    if (definition == nullptr) {
        return {false, "Use item rejected: unknown item."};
    }
    if (StoredItemAmountForPlayer(world, controller_player_id, item_id) < 1.0f) {
        return {false, "Use item rejected: item is not available in owned storage."};
    }

    std::size_t affected = 0;
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Use item denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        unit.stamina = std::min(unit.max_stamina, unit.stamina + 12.0f);
        unit.tactical_state = std::string("Using ") + definition->display_name;
        ++affected;
    }

    std::ostringstream message;
    message << "Used " << definition->display_name << " with " << affected << " unit(s).";
    return {true, message.str(), item_id};
}

CommandResult IssueHoldPositionOrder(
    WorldState& world,
    dbd::Id controller_player_id,
    const std::vector<dbd::Id>& unit_ids,
    const dbd::Vec3& position) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for hold_position."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Hold position denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearQueuedOrders(unit);
        unit.current_order = dbd::UnitOrderType::Escort;
        unit.assignment.kind = dbd::JobKind::Escort;
        unit.assignment.target_entity_id = 0;
        unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
        unit.assignment.target_position = position;
        unit.move_target = position;
    }
    return {true, "Hold position order accepted."};
}

CommandResult IssueStopOrder(WorldState& world, dbd::Id controller_player_id, const std::vector<dbd::Id>& unit_ids) {
    if (unit_ids.empty()) {
        return {false, "No units supplied for stop."};
    }
    for (dbd::Id unit_id : unit_ids) {
        if (!HasControl(world, controller_player_id, unit_id, dbd::AuthorityPermission::IssueMove)) {
            return {false, "Stop denied for one or more units."};
        }
    }
    for (dbd::Id unit_id : unit_ids) {
        auto& unit = world.units.at(unit_id);
        RemoveUnitFromAssignedJobs(world, unit_id);
        ClearUnitAssignment(unit);
        ClearQueuedOrders(unit);
        unit.move_target = unit.position;
    }
    return {true, "Stop order accepted."};
}

CommandResult ProgressGuardOrders(WorldState& world, float delta_seconds) {
    (void)delta_seconds;
    std::size_t guarded_units = 0;
    std::size_t responses = 0;

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || unit.current_order != dbd::UnitOrderType::Escort ||
            unit.assignment.kind != dbd::JobKind::Escort) {
            continue;
        }

        const auto anchor = ResolveGuardAnchor(world, unit);
        if (!anchor.has_value()) {
            ClearUnitAssignment(unit);
            continue;
        }

        ++guarded_units;
        unit.assignment.target_position = *anchor;
        const float guard_radius = unit.assignment.target_entity_id == 0 ? 10.0f : 12.0f;
        const auto enemy = FindNearestEnemyUnitNearPoint(world, unit.owner_player_id, *anchor, guard_radius);
        if (enemy.has_value()) {
            const auto enemy_it = world.units.find(*enemy);
            if (enemy_it != world.units.end()) {
                const float range = MaxAttackRangeFor(unit);
                const float distance_to_enemy_sq = DistanceSquared(unit.position, enemy_it->second.position);
                if (distance_to_enemy_sq <= range * range) {
                    const float damage = BaseCombatDamagePerSecond(unit) * EconomicCombatMultiplier(world, unit) * delta_seconds;
                    auto& target = world.units.at(*enemy);
                    target.health = std::max(0.0f, target.health - damage);
                    InterruptUnitWork(world, target);
                    if (target.health <= 0.0f) {
                        ResolvePermanentUnitDeath(world, target.unit_id);
                    }
                    ++responses;
                    continue;
                }
                if (distance_to_enemy_sq <= (guard_radius * guard_radius)) {
                    unit.move_target = enemy_it->second.position;
                    ++responses;
                    continue;
                }
            }
        }

        const float desired_distance = unit.assignment.target_entity_id == 0 ? 0.5f : 5.0f;
        if (DistanceSquared(unit.position, *anchor) > desired_distance * desired_distance) {
            unit.move_target = *anchor;
        } else {
            unit.move_target = unit.position;
        }
    }

    std::ostringstream message;
    message << "Guard orders progressed for " << guarded_units << " unit(s), " << responses << " defensive response(s).";
    return {true, message.str()};
}

CommandResult ProgressRepairOrders(WorldState& world, float delta_seconds) {
    std::size_t repairing_units = 0;
    std::size_t stalled_units = 0;
    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead ||
            unit.current_order != dbd::UnitOrderType::BuildSite ||
            unit.assignment.kind != dbd::JobKind::Build) {
            continue;
        }

        float* target_health = nullptr;
        float target_max_health = 0.0f;
        dbd::Vec3 target_position {};
        dbd::Id owner_player_id = 0;
        bool valid_target = false;

        if (unit.assignment.target_kind == dbd::AttackTargetKind::Structure) {
            auto structure_it = world.structures.find(unit.assignment.target_entity_id);
            if (structure_it != world.structures.end() && structure_it->second.owner_player_id == unit.owner_player_id) {
                target_health = &structure_it->second.health;
                target_max_health = structure_it->second.max_health;
                target_position = structure_it->second.position;
                owner_player_id = structure_it->second.owner_player_id;
                valid_target = true;
            }
        } else if (unit.assignment.target_kind == dbd::AttackTargetKind::ConstructionSite) {
            auto site_it = world.construction_sites.find(unit.assignment.target_entity_id);
            if (site_it != world.construction_sites.end() &&
                site_it->second.owner_player_id == unit.owner_player_id &&
                site_it->second.stage != dbd::ConstructionStage::Destroyed &&
                site_it->second.stage != dbd::ConstructionStage::Canceled &&
                site_it->second.stage != dbd::ConstructionStage::Completed) {
                target_health = &site_it->second.health;
                target_max_health = site_it->second.max_health;
                target_position = site_it->second.position;
                owner_player_id = site_it->second.owner_player_id;
                valid_target = true;
            }
        }

        if (!valid_target || target_health == nullptr) {
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
            unit.move_target = unit.position;
            unit.tactical_state = "Repair target lost";
            continue;
        }

        unit.assignment.target_position = target_position;
        if (Distance(unit.position, target_position) > 2.5f) {
            unit.move_target = target_position;
            continue;
        }

        if (*target_health >= target_max_health) {
            ClearUnitAssignment(unit);
            unit.current_order = dbd::UnitOrderType::Idle;
            unit.move_target = unit.position;
            unit.tactical_state = "Repair complete";
            continue;
        }

        const float labor = ConstructionLaborPerSecond(unit) * EconomicLaborMultiplier(world, unit) * delta_seconds;
        const float max_repair = std::min(target_max_health - *target_health, labor * 5.0f);
        const float material_needed = std::max(0.1f, max_repair * 0.05f);
        float materials = ConsumeOwnedRepairMaterials(world, owner_player_id, material_needed);
        if (materials < material_needed) {
            materials += ConsumeNearbyRepairSupply(world, owner_player_id, target_position, material_needed - materials);
        }
        if (materials <= 0.0f) {
            unit.tactical_state = "Repair stalled: no materials";
            ++stalled_units;
            continue;
        }

        const float material_ratio = std::clamp(materials / material_needed, 0.0f, 1.0f);
        const float repaired = max_repair * material_ratio;
        *target_health = std::min(target_max_health, *target_health + repaired);
        AwardConstructionXp(unit, labor * material_ratio);
        unit.tactical_state = unit.assignment.target_kind == dbd::AttackTargetKind::Structure
            ? "Repairing structure"
            : "Repairing site";
        ++repairing_units;
    }

    std::ostringstream message;
    message << "Repair progressed for " << repairing_units << " unit(s)";
    if (stalled_units > 0) {
        message << ", stalled " << stalled_units << " unit(s) without materials";
    }
    message << '.';
    return {true, message.str()};
}

CommandResult ProgressUnitMovement(WorldState& world, float delta_seconds) {
    std::size_t moved_units = 0;
    std::size_t delivered_units = 0;

    for (auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }

        if (unit.current_order == dbd::UnitOrderType::Harvest && unit.assignment.target_entity_id != 0) {
            if (unit.assignment.kind == dbd::JobKind::Harvest) {
                const auto node_it = world.resource_nodes.find(unit.assignment.target_entity_id);
                if (node_it == world.resource_nodes.end()) {
                    ClearUnitAssignment(unit);
                    unit.current_order = dbd::UnitOrderType::Idle;
                    unit.move_target = unit.position;
                    unit.tactical_state = "Harvest target lost";
                    continue;
                }
                unit.assignment.target_position = node_it->second.position;
                unit.move_target = node_it->second.position;
                if (Distance(unit.position, node_it->second.position) <= kInteractionRange) {
                    CompleteHarvestInteraction(world, unit_id, unit.assignment.target_entity_id);
                    continue;
                }
            } else if (unit.assignment.route_source_kind == dbd::HaulRouteSourceKind::DroppedCargo) {
                const auto drop_it = world.dropped_cargo.find(unit.assignment.target_entity_id);
                if (drop_it == world.dropped_cargo.end()) {
                    ClearUnitAssignment(unit);
                    unit.current_order = dbd::UnitOrderType::Idle;
                    unit.move_target = unit.position;
                    unit.tactical_state = "Loot target lost";
                    continue;
                }
                unit.assignment.target_position = drop_it->second.position;
                unit.move_target = drop_it->second.position;
                if (Distance(unit.position, drop_it->second.position) <= kInteractionRange) {
                    CompleteLootInteraction(world, unit_id, unit.assignment.target_entity_id);
                    continue;
                }
            }
        }

        const bool can_move =
            unit.current_order == dbd::UnitOrderType::Move ||
            unit.current_order == dbd::UnitOrderType::Harvest ||
            unit.current_order == dbd::UnitOrderType::Scout ||
            unit.current_order == dbd::UnitOrderType::Escort ||
            unit.current_order == dbd::UnitOrderType::BuildSite ||
            unit.current_order == dbd::UnitOrderType::AttackTarget ||
            unit.current_order == dbd::UnitOrderType::HaulToStorage ||
            unit.current_order == dbd::UnitOrderType::Retreat;

        ApplyMovementStaminaCost(unit, delta_seconds);

        if (!can_move) {
            continue;
        }

        if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
            unit.assignment.target_entity_id != 0 &&
            !unit.assignment.route_active) {
            const auto storage_it = world.storage_sites.find(unit.assignment.target_entity_id);
            if (storage_it != world.storage_sites.end() && Distance(unit.position, storage_it->second.position) <= 5.25f) {
                const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.target_entity_id);
                if (delivery.ok) {
                    ++delivered_units;
                    ClearUnitAssignment(unit);
                    TryStartNextQueuedOrder(unit);
                    continue;
                }
            }
        }

        const float distance = Distance(unit.position, unit.move_target);
        if (distance <= 0.05f) {
            if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                unit.assignment.target_entity_id != 0 &&
                !unit.assignment.route_active) {
                const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.target_entity_id);
                if (delivery.ok) {
                    ++delivered_units;
                    ClearUnitAssignment(unit);
                    TryStartNextQueuedOrder(unit);
                }
            } else if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                       unit.assignment.route_active) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::Escort) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::BuildSite) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::Scout &&
                       unit.assignment.kind == dbd::JobKind::Scout) {
                if (unit.assignment.patrol_route) {
                    unit.assignment.patrol_to_secondary = !unit.assignment.patrol_to_secondary;
                    unit.move_target = unit.assignment.patrol_to_secondary
                        ? unit.assignment.secondary_position
                        : unit.assignment.target_position;
                    continue;
                }
                if (unit.assignment.radius > 0.0f) {
                    unit.assignment.patrol_to_secondary = !unit.assignment.patrol_to_secondary;
                    unit.move_target = unit.assignment.patrol_to_secondary
                        ? unit.assignment.secondary_position
                        : unit.assignment.target_position;
                    continue;
                }
            } else {
                ClearUnitAssignment(unit);
                TryStartNextQueuedOrder(unit);
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

        const dbd::Vec3 next_position {
            unit.position.x + (direction.x * step),
            unit.position.y + (direction.y * step),
            unit.position.z + (direction.z * step)};

        if (MovementWouldCollide(world, unit_id, unit.position, next_position, unit.move_target)) {
            const auto detour = FindSimpleDetourStep(world, unit_id, unit.position, unit.move_target, direction, step);
            if (!detour.has_value()) {
                unit.tactical_state = "Path blocked";
                continue;
            }
            unit.position = *detour;
            unit.tactical_state = "Pathing around obstacle";
            GetOrCreateChunk(world, WorldToChunk(unit.position));
            ++moved_units;
            continue;
        }

        unit.position = next_position;
        GetOrCreateChunk(world, WorldToChunk(unit.position));
        ++moved_units;

        if (unit.current_order == dbd::UnitOrderType::Harvest && unit.assignment.target_entity_id != 0) {
            if (unit.assignment.kind == dbd::JobKind::Harvest) {
                const auto node_it = world.resource_nodes.find(unit.assignment.target_entity_id);
                if (node_it != world.resource_nodes.end() &&
                    Distance(unit.position, node_it->second.position) <= kInteractionRange) {
                    CompleteHarvestInteraction(world, unit_id, unit.assignment.target_entity_id);
                    continue;
                }
            } else if (unit.assignment.route_source_kind == dbd::HaulRouteSourceKind::DroppedCargo) {
                const auto drop_it = world.dropped_cargo.find(unit.assignment.target_entity_id);
                if (drop_it != world.dropped_cargo.end() &&
                    Distance(unit.position, drop_it->second.position) <= kInteractionRange) {
                    CompleteLootInteraction(world, unit_id, unit.assignment.target_entity_id);
                    continue;
                }
            }
        }

        if (Distance(unit.position, unit.move_target) <= 0.25f) {
            unit.position = unit.move_target;
            if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                unit.assignment.target_entity_id != 0 &&
                !unit.assignment.route_active) {
                const auto delivery = DeliverCargoToStorage(world, unit_id, unit.assignment.target_entity_id);
                if (delivery.ok) {
                    ++delivered_units;
                }
            } else if (unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                       unit.assignment.route_active) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::Escort) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::BuildSite) {
                continue;
            } else if (unit.current_order == dbd::UnitOrderType::Scout &&
                       unit.assignment.kind == dbd::JobKind::Scout) {
                if (unit.assignment.patrol_route) {
                    unit.assignment.patrol_to_secondary = !unit.assignment.patrol_to_secondary;
                    unit.move_target = unit.assignment.patrol_to_secondary
                        ? unit.assignment.secondary_position
                        : unit.assignment.target_position;
                    continue;
                }
                if (unit.assignment.radius > 0.0f) {
                    unit.assignment.patrol_to_secondary = !unit.assignment.patrol_to_secondary;
                    unit.move_target = unit.assignment.patrol_to_secondary
                        ? unit.assignment.secondary_position
                        : unit.assignment.target_position;
                    continue;
                }
            }
            ClearUnitAssignment(unit);
            TryStartNextQueuedOrder(unit);
        }
    }

    std::ostringstream message;
    message << "Movement progressed for " << moved_units << " unit(s)";
    if (delivered_units > 0) {
        message << ", " << delivered_units << " auto-delivery completion(s)";
    }
    message << '.';
    if (moved_units > 0) {
        MarkUnitSpatialIndexDirty(world);
    }
    return {true, message.str()};
}

CommandResult RefreshKnownContacts(WorldState& world) {
    for (auto& [_, player_contacts] : world.known_contacts) {
        for (auto& [__, contact] : player_contacts) {
            contact.currently_visible = false;
        }
    }

    std::size_t visible_contacts = 0;
    auto note_contact = [&](dbd::Id observer_player_id,
                            dbd::Id target_id,
                            dbd::AttackTargetKind target_kind,
                            const dbd::Vec3& position,
                            dbd::Id spotted_by_unit_id) {
        auto& contact = world.known_contacts[observer_player_id][target_id];
        contact.observing_player_id = observer_player_id;
        contact.target_entity_id = target_id;
        contact.target_kind = target_kind;
        contact.position = position;
        contact.last_seen_tick = world.tick;
        contact.spotted_by_unit_id = spotted_by_unit_id;
        contact.currently_visible = true;
        ++visible_contacts;
    };

    for (const auto& [observer_id, observer] : world.units) {
        if (!observer.alive || observer.permanently_dead) {
            continue;
        }

        const float vision_range = VisionRangeFor(observer);
        const float vision_sq = vision_range * vision_range;

        for (dbd::Id target_id : FindNearbyUnitIds(world, observer.position, vision_range)) {
            const auto target_it = world.units.find(target_id);
            if (target_it == world.units.end()) {
                continue;
            }
            const auto& target = target_it->second;
            if (!target.alive || target.permanently_dead || target_id == observer_id ||
                target.owner_player_id == observer.owner_player_id) {
                continue;
            }
            if (DistanceSquared(observer.position, target.position) <= vision_sq) {
                note_contact(observer.owner_player_id, target_id, dbd::AttackTargetKind::Unit, target.position, observer_id);
            }
        }

        for (const auto& [site_id, site] : world.construction_sites) {
            if (site.owner_player_id == observer.owner_player_id ||
                site.stage == dbd::ConstructionStage::Destroyed ||
                site.stage == dbd::ConstructionStage::Completed) {
                continue;
            }
            if (DistanceSquared(observer.position, site.position) <= vision_sq) {
                note_contact(observer.owner_player_id, site_id, dbd::AttackTargetKind::ConstructionSite, site.position, observer_id);
            }
        }

        for (const auto& [structure_id, structure] : world.structures) {
            if (structure.owner_player_id == observer.owner_player_id) {
                continue;
            }
            if (DistanceSquared(observer.position, structure.position) <= vision_sq) {
                note_contact(observer.owner_player_id, structure_id, dbd::AttackTargetKind::Structure, structure.position, observer_id);
            }
        }
    }

    for (auto player_it = world.known_contacts.begin(); player_it != world.known_contacts.end();) {
        for (auto contact_it = player_it->second.begin(); contact_it != player_it->second.end();) {
            if (!contact_it->second.currently_visible &&
                world.tick > contact_it->second.last_seen_tick + kContactRetentionTicks) {
                contact_it = player_it->second.erase(contact_it);
            } else {
                ++contact_it;
            }
        }

        if (player_it->second.empty()) {
            player_it = world.known_contacts.erase(player_it);
        } else {
            ++player_it;
        }
    }

    std::ostringstream message;
    message << "Known contacts refreshed with " << visible_contacts << " currently visible contact(s).";
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
            if (!unit.assignment.delegated &&
                world.squads.find(unit.squad_id) != world.squads.end() &&
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
            for (auto& [_, policy] : world.insurance_policies) {
                if (policy.active && policy.insured_entity_id == structure.structure_id) {
                    insurance_payout += ComputeInsurancePayout(policy, structure.max_health * 2.0f);
                    policy.active = false;
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
        UpdatePlayerEconomicTelemetry(world, player_id, player, true);

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

CommandResult RefreshEconomicTelemetry(WorldState& world) {
    for (auto& [player_id, player] : world.players) {
        UpdatePlayerEconomicTelemetry(world, player_id, player, false);
    }
    return {true, "Economic telemetry refreshed."};
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
        const bool tactical_move_with_queue =
            !unit.queued_orders.empty() &&
            (unit.current_order == dbd::UnitOrderType::Move ||
             unit.current_order == dbd::UnitOrderType::Scout ||
             unit.current_order == dbd::UnitOrderType::HaulToStorage);
        if (unit.assignment.delegated &&
            (unit.current_order == dbd::UnitOrderType::AttackTarget || unit.current_order == dbd::UnitOrderType::Move) &&
            health_ratio > 0.25f) {
            const bool low_health_override = std::any_of(unit.automation_rules.begin(), unit.automation_rules.end(), [&](const auto& rule) {
                return rule.enabled && rule.trigger == dbd::AutomationTrigger::LowHealth &&
                       health_ratio <= rule.threshold &&
                       rule.action == dbd::AutomationAction::Retreat;
            });
            if (!low_health_override) {
                continue;
            }
        }

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
                    RemoveUnitFromAssignedJobs(world, unit.unit_id);
                    ClearQueueForTacticalOverride(unit, health_ratio <= rule.threshold ? "Retreat override" : "Queue interrupted: enemy seen");
                    unit.current_order = dbd::UnitOrderType::Retreat;
                    unit.assignment = {};
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
                            RemoveUnitFromAssignedJobs(world, unit.unit_id);
                            ClearQueueForTacticalOverride(unit, "Queue interrupted: storage return");
                            unit.current_order = dbd::UnitOrderType::HaulToStorage;
                            unit.assignment = {};
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
                    RemoveUnitFromAssignedJobs(world, unit.unit_id);
                    ClearQueueForTacticalOverride(unit, tactical_move_with_queue ? "Queue held: enemy seen" : "Holding position");
                    unit.current_order = dbd::UnitOrderType::Escort;
                    unit.assignment = {};
                    unit.assignment.kind = dbd::JobKind::Escort;
                    unit.assignment.target_entity_id = 0;
                    unit.assignment.target_kind = dbd::AttackTargetKind::Unit;
                    unit.assignment.target_position = unit.position;
                    unit.move_target = unit.position;
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
                        RemoveUnitFromAssignedJobs(world, unit.unit_id);
                        ClearQueueForTacticalOverride(unit, tactical_move_with_queue ? "Queue interrupted: enemy seen" : "Enemy engaged");
                        unit.current_order = dbd::UnitOrderType::AttackTarget;
                        unit.assignment = {};
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
                RemoveUnitFromAssignedJobs(world, unit_id);
                ClearQueueForTacticalOverride(unit_it->second, "Retreat override");
                unit_it->second.current_order = dbd::UnitOrderType::Retreat;
                unit_it->second.assignment = {};
                unit_it->second.assignment.kind = dbd::JobKind::Retreat;
                unit_it->second.move_target = ComputeRetreatTarget(world, unit_it->second);
                unit_it->second.assignment.target_position = unit_it->second.move_target;
            } else if (squad.stance == dbd::SquadStance::Defensive &&
                       unit_it->second.current_order == dbd::UnitOrderType::AttackTarget &&
                       !EnemySeenNearby(world, unit_it->second)) {
                RemoveUnitFromAssignedJobs(world, unit_id);
                unit_it->second.current_order = dbd::UnitOrderType::Idle;
                ClearUnitAssignment(unit_it->second);
                MarkTacticalState(unit_it->second, "Defensive disengage");
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
