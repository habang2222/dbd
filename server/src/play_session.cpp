#include "dbd_server/command_spool.hpp"
#include "dbd_server/server_simulation.hpp"
#include "dbd_server/world_bootstrap.hpp"
#include "dbd_server/world_state.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint64_t kDepotRunTimeLimitTicks = 600'000;

std::filesystem::path TempOutputPath(const std::filesystem::path& output_path) {
    auto temp_path = output_path;
    temp_path += ".tmp";
    return temp_path;
}

bool CommitTempOutputFile(const std::filesystem::path& temp_path, const std::filesystem::path& output_path) {
    std::error_code ec;
    std::filesystem::remove(output_path, ec);
    ec.clear();
    std::filesystem::rename(temp_path, output_path, ec);
    if (ec) {
        std::error_code remove_ec;
        std::filesystem::remove(temp_path, remove_ec);
        return false;
    }
    return true;
}

struct SessionCommand {
    std::string command_id {};
    std::string type;
    dbd::Id controller_player_id {};
    dbd::Id player_id {};
    dbd::Id squad_id {};
    std::vector<dbd::Id> unit_ids {};
    dbd::Id unit_id {};
    dbd::Id target_entity_id {};
    dbd::Id resource_node_id {};
    dbd::Id dropped_cargo_id {};
    dbd::Id storage_site_id {};
    dbd::Id item_id {};
    std::uint32_t amount {1};
    dbd::HaulRouteSourceKind source_kind {dbd::HaulRouteSourceKind::None};
    dbd::Id source_id {};
    dbd::AttackTargetKind target_kind {dbd::AttackTargetKind::Unit};
    dbd::StructureType structure_type {dbd::StructureType::StorageDepot};
    dbd::Vec3 target_position {};
    dbd::Vec3 center {};
    dbd::Vec3 position {};
    dbd::Vec3 point_a {};
    dbd::Vec3 point_b {};
    float radius {};
    float target_grade {0.12f};
    float footprint_radius {8.0f};
    std::vector<dbd::AutomationRule> automation_rules {};
};

struct DepotRunObjective {
    std::string name {"Depot Run v0"};
    std::string state {"active"};
    std::string reason {"in_progress"};
    std::string summary {};
    float iron_fitting_stored {};
    float iron_fitting_goal {15.0f};
    dbd::Id iron_node_id {};
    dbd::Vec3 iron_node_position {};
    bool forward_depot_complete {};
    std::size_t living_units {};
    std::size_t units_lost {};
    float primary_depot_health {};
    float primary_depot_max_health {};
    std::uint64_t time_limit_ticks {kDepotRunTimeLimitTicks};
    std::uint64_t remaining_ticks {kDepotRunTimeLimitTicks};
};

std::optional<float> ExtractFloat(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    auto start = object_text.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    auto end = object_text.find_first_not_of("0123456789.-", start);
    try {
        return std::stof(object_text.substr(start, end - start));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<dbd::Id> ExtractId(const std::string& object_text, const std::string& key) {
    const auto value = ExtractFloat(object_text, key);
    if (!value.has_value() || *value < 0.0f) {
        return std::nullopt;
    }
    return static_cast<dbd::Id>(*value);
}

std::optional<bool> ExtractBool(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const auto start = object_text.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    if (object_text.compare(start, 4, "true") == 0) {
        return true;
    }
    if (object_text.compare(start, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

std::vector<dbd::Id> ExtractIdArray(const std::string& object_text, const std::string& key) {
    std::vector<dbd::Id> ids;
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return ids;
    }
    const auto bracket_start = object_text.find('[', key_pos + needle.size());
    const auto bracket_end = object_text.find(']', bracket_start + 1);
    if (bracket_start == std::string::npos || bracket_end == std::string::npos) {
        return ids;
    }
    std::stringstream stream(object_text.substr(bracket_start + 1, bracket_end - bracket_start - 1));
    std::string token;
    while (std::getline(stream, token, ',')) {
        std::stringstream trim(token);
        double value = 0.0;
        if (trim >> value) {
            ids.push_back(static_cast<dbd::Id>(value));
        }
    }
    return ids;
}

std::optional<dbd::Vec3> ExtractVec3(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto object_start = object_text.find('{', key_pos + needle.size());
    const auto object_end = object_text.find('}', object_start + 1);
    if (object_start == std::string::npos || object_end == std::string::npos) {
        return std::nullopt;
    }
    const std::string inner = object_text.substr(object_start, object_end - object_start + 1);
    dbd::Vec3 value {};
    value.x = ExtractFloat(inner, "x").value_or(0.0f);
    value.y = ExtractFloat(inner, "y").value_or(0.0f);
    value.z = ExtractFloat(inner, "z").value_or(0.0f);
    return value;
}

dbd::AttackTargetKind ParseTargetKind(const std::string& raw) {
    if (raw == "ConstructionSite") {
        return dbd::AttackTargetKind::ConstructionSite;
    }
    if (raw == "Structure") {
        return dbd::AttackTargetKind::Structure;
    }
    return dbd::AttackTargetKind::Unit;
}

dbd::HaulRouteSourceKind ParseHaulRouteSourceKind(const std::string& raw) {
    if (raw == "ResourceNode" || raw == "resource_node") {
        return dbd::HaulRouteSourceKind::ResourceNode;
    }
    if (raw == "DroppedCargo" || raw == "dropped_cargo") {
        return dbd::HaulRouteSourceKind::DroppedCargo;
    }
    return dbd::HaulRouteSourceKind::None;
}

dbd::StructureType ParseStructureType(const std::string& raw) {
    if (raw == "Extractor") {
        return dbd::StructureType::Extractor;
    }
    if (raw == "DefenseNode") {
        return dbd::StructureType::DefenseNode;
    }
    return dbd::StructureType::StorageDepot;
}

dbd::AutomationTrigger ParseAutomationTrigger(const std::string& raw) {
    if (raw == "InventoryHeavy") {
        return dbd::AutomationTrigger::InventoryHeavy;
    }
    if (raw == "InventoryFull") {
        return dbd::AutomationTrigger::InventoryFull;
    }
    if (raw == "EnemySeen") {
        return dbd::AutomationTrigger::EnemySeen;
    }
    return dbd::AutomationTrigger::LowHealth;
}

dbd::AutomationAction ParseAutomationAction(const std::string& raw) {
    if (raw == "ReturnToStorage") {
        return dbd::AutomationAction::ReturnToStorage;
    }
    if (raw == "HoldPosition") {
        return dbd::AutomationAction::HoldPosition;
    }
    if (raw == "AttackNearestEnemy") {
        return dbd::AutomationAction::AttackNearestEnemy;
    }
    return dbd::AutomationAction::Retreat;
}

std::optional<std::string> ExtractArrayText(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto array_start = object_text.find('[', key_pos + needle.size());
    if (array_start == std::string::npos) {
        return std::nullopt;
    }

    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t i = array_start; i < object_text.size(); ++i) {
        const char ch = object_text[i];
        if (escaping) {
            escaping = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return object_text.substr(array_start, i - array_start + 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> SplitTopLevelObjectsFromArray(const std::string& array_text) {
    std::vector<std::string> objects;
    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    std::size_t object_start = std::string::npos;
    for (std::size_t i = 0; i < array_text.size(); ++i) {
        const char ch = array_text[i];
        if (escaping) {
            escaping = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                object_start = i;
            }
            ++depth;
        } else if (ch == '}') {
            if (depth == 0) {
                return {};
            }
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(array_text.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }
    if (depth != 0 || in_string || escaping) {
        return {};
    }
    return objects;
}

std::vector<dbd::AutomationRule> ExtractAutomationRules(const std::string& object_text, const std::string& key) {
    std::vector<dbd::AutomationRule> rules;
    const auto array_text = ExtractArrayText(object_text, key);
    if (!array_text.has_value()) {
        return rules;
    }

    for (const auto& rule_text : SplitTopLevelObjectsFromArray(*array_text)) {
        dbd::AutomationRule rule {};
        if (const auto trigger = dbd_server::ExtractQuotedString(rule_text, "trigger"); trigger.has_value()) {
            rule.trigger = ParseAutomationTrigger(*trigger);
        } else if (const auto trigger_id = ExtractId(rule_text, "trigger"); trigger_id.has_value()) {
            rule.trigger = static_cast<dbd::AutomationTrigger>(*trigger_id);
        }
        if (const auto action = dbd_server::ExtractQuotedString(rule_text, "action"); action.has_value()) {
            rule.action = ParseAutomationAction(*action);
        } else if (const auto action_id = ExtractId(rule_text, "action"); action_id.has_value()) {
            rule.action = static_cast<dbd::AutomationAction>(*action_id);
        }
        rule.threshold = ExtractFloat(rule_text, "threshold").value_or(rule.threshold);
        rule.enabled = ExtractBool(rule_text, "enabled").value_or(rule.enabled);
        rules.push_back(rule);
    }

    return rules;
}

std::vector<std::string> SplitCommandObjects(const std::string& raw) {
    std::vector<std::string> objects;
    const auto commands_pos = raw.find("\"commands\"");
    if (commands_pos == std::string::npos) {
        return objects;
    }
    const auto array_start = raw.find('[', commands_pos);
    if (array_start == std::string::npos) {
        return objects;
    }

    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    std::size_t object_start = std::string::npos;
    for (std::size_t i = array_start + 1; i < raw.size(); ++i) {
        const char ch = raw[i];
        if (escaping) {
            escaping = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                object_start = i;
            }
            ++depth;
        } else if (ch == '}') {
            if (depth == 0) {
                return {};
            }
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(raw.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    if (depth != 0 || in_string || escaping) {
        return {};
    }
    return objects;
}

std::optional<SessionCommand> ParseCommandObject(const std::string& object_text) {
    const auto type = dbd_server::ExtractQuotedString(object_text, "type");
    if (!type.has_value()) {
        return std::nullopt;
    }

    SessionCommand command {};
    command.command_id = dbd_server::ExtractQuotedString(object_text, "commandId").value_or("");
    command.type = *type;
    command.controller_player_id = ExtractId(object_text, "controllerPlayerId").value_or(0);
    command.player_id = ExtractId(object_text, "playerId").value_or(command.controller_player_id);
    command.squad_id = ExtractId(object_text, "squadId").value_or(0);
    command.unit_ids = ExtractIdArray(object_text, "unitIds");
    command.unit_id = ExtractId(object_text, "unitId").value_or(0);
    command.target_entity_id = ExtractId(object_text, "targetEntityId").value_or(0);
    command.resource_node_id = ExtractId(object_text, "resourceNodeId").value_or(0);
    command.dropped_cargo_id = ExtractId(object_text, "droppedCargoId").value_or(0);
    command.storage_site_id = ExtractId(object_text, "storageSiteId").value_or(0);
    command.item_id = ExtractId(object_text, "itemId").value_or(0);
    command.amount = static_cast<std::uint32_t>(ExtractFloat(object_text, "amount").value_or(1.0f));
    command.source_id = ExtractId(object_text, "sourceId").value_or(0);
    command.radius = ExtractFloat(object_text, "radius").value_or(6.0f);
    command.target_grade = ExtractFloat(object_text, "targetGrade").value_or(0.12f);
    command.footprint_radius = ExtractFloat(object_text, "footprintRadius").value_or(8.0f);
    command.target_kind = ParseTargetKind(dbd_server::ExtractQuotedString(object_text, "targetKind").value_or("Unit"));
    command.source_kind = ParseHaulRouteSourceKind(dbd_server::ExtractQuotedString(object_text, "sourceKind").value_or("None"));
    command.structure_type = ParseStructureType(dbd_server::ExtractQuotedString(object_text, "structureType").value_or("StorageDepot"));
    command.target_position = ExtractVec3(object_text, "target").value_or(dbd::Vec3 {});
    command.center = ExtractVec3(object_text, "center").value_or(dbd::Vec3 {});
    command.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
    command.point_a = ExtractVec3(object_text, "pointA").value_or(dbd::Vec3 {});
    command.point_b = ExtractVec3(object_text, "pointB").value_or(dbd::Vec3 {});
    command.automation_rules = ExtractAutomationRules(object_text, "automationRules");
    return command;
}

std::vector<SessionCommand> ParseCommandPayload(const std::string& raw) {
    std::vector<SessionCommand> commands;
    if (raw.find("\"commands\"") == std::string::npos) {
        const auto direct_command = ParseCommandObject(raw);
        if (!direct_command.has_value()) {
            return commands;
        }
        commands.push_back(*direct_command);
        return commands;
    }

    for (const auto& object_text : SplitCommandObjects(raw)) {
        if (const auto command = ParseCommandObject(object_text); command.has_value()) {
            commands.push_back(*command);
        }
    }
    return commands;
}

float DistanceSquared(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

float CargoWeight(const std::vector<dbd::CargoStack>& cargo) {
    float total = 0.0f;
    for (const auto& stack : cargo) {
        total += static_cast<float>(stack.amount) * stack.unit_weight;
    }
    return total;
}

float StoredItemAmountForPlayer(const dbd_server::WorldState& world, dbd::Id player_id, dbd::Id item_id) {
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

std::size_t LivingUnitCountForPlayer(const dbd_server::WorldState& world, dbd::Id player_id) {
    std::size_t living_units = 0;
    for (const auto& [_, unit] : world.units) {
        if (unit.owner_player_id == player_id && unit.alive && !unit.permanently_dead) {
            ++living_units;
        }
    }
    return living_units;
}

bool HasForwardDepotComplete(
    const dbd_server::WorldState& world,
    dbd::Id player_id,
    dbd::Id primary_depot_structure_id) {
    for (const auto& [structure_id, structure] : world.structures) {
        if (structure_id == primary_depot_structure_id ||
            structure.owner_player_id != player_id ||
            structure.structure_type != dbd::StructureType::StorageDepot ||
            structure.health <= 0.0f) {
            continue;
        }
        return true;
    }
    for (const auto& [_, site] : world.construction_sites) {
        if (site.owner_player_id == player_id &&
            site.structure_type == dbd::StructureType::StorageDepot &&
            site.stage == dbd::ConstructionStage::Completed) {
            return true;
        }
    }
    return false;
}

std::optional<std::pair<dbd::Id, dbd::Vec3>> BestIronFittingNode(const dbd_server::WorldState& world) {
    std::optional<std::pair<dbd::Id, dbd::Vec3>> best;
    float best_score = 0.0f;
    for (const auto& [node_id, node] : world.resource_nodes) {
        if (node.risk_band != dbd::RegionRiskBand::High || node.remaining_amount == 0) {
            continue;
        }
        const float score = node.richness * node.extraction_rate;
        if (!best.has_value() || score > best_score) {
            best = std::make_pair(node_id, node.position);
            best_score = score;
        }
    }
    return best;
}

DepotRunObjective BuildDepotRunObjective(
    const dbd_server::WorldState& world,
    dbd::Id primary_player_id,
    dbd::Id primary_depot_structure_id,
    std::uint64_t tick) {
    DepotRunObjective objective {};
    objective.iron_fitting_stored = StoredItemAmountForPlayer(world, primary_player_id, 91'003);
    if (const auto iron_node = BestIronFittingNode(world); iron_node.has_value()) {
        objective.iron_node_id = iron_node->first;
        objective.iron_node_position = iron_node->second;
    }
    objective.forward_depot_complete = HasForwardDepotComplete(world, primary_player_id, primary_depot_structure_id);
    objective.living_units = LivingUnitCountForPlayer(world, primary_player_id);
    objective.units_lost = objective.living_units >= 4 ? 0 : 4 - objective.living_units;
    objective.remaining_ticks = tick >= objective.time_limit_ticks ? 0 : objective.time_limit_ticks - tick;

    const bool primary_depot_alive = world.structures.find(primary_depot_structure_id) != world.structures.end();
    if (primary_depot_alive) {
        const auto& primary_depot = world.structures.at(primary_depot_structure_id);
        objective.primary_depot_health = primary_depot.health;
        objective.primary_depot_max_health = primary_depot.max_health;
    }
    if (objective.iron_fitting_stored >= objective.iron_fitting_goal) {
        objective.state = "success";
        objective.reason = "iron_secured";
        objective.summary = "Success: Iron Fitting secured.";
    } else if (objective.forward_depot_complete) {
        objective.state = "success";
        objective.reason = "forward_depot_complete";
        objective.summary = "Success: forward depot completed.";
    } else if (!primary_depot_alive) {
        objective.state = "failed";
        objective.reason = "primary_depot_lost";
        objective.summary = "Failed: primary depot lost.";
    } else if (objective.living_units == 0) {
        objective.state = "failed";
        objective.reason = "all_founder_units_lost";
        objective.summary = "Failed: all founder units lost.";
    } else if (tick >= objective.time_limit_ticks) {
        objective.state = "failed";
        objective.reason = "time_expired";
        objective.summary = "Failed: depot run timer expired.";
    } else {
        std::ostringstream out;
        out << "Store Iron Fitting " << static_cast<int>(std::floor(objective.iron_fitting_stored))
            << "/" << static_cast<int>(objective.iron_fitting_goal)
            << " or complete a forward depot.";
        objective.summary = out.str();
    }
    return objective;
}

std::vector<dbd::Id> CollectLivingUnits(const dbd_server::WorldState& world, dbd::Id player_id) {
    std::vector<dbd::Id> unit_ids;
    const auto player_it = world.players.find(player_id);
    if (player_it == world.players.end()) {
        return unit_ids;
    }
    for (dbd::Id unit_id : player_it->second.unit_ids) {
        const auto unit_it = world.units.find(unit_id);
        if (unit_it == world.units.end() || !unit_it->second.alive || unit_it->second.permanently_dead) {
            continue;
        }
        unit_ids.push_back(unit_id);
    }
    return unit_ids;
}

void ApplyMidfieldRaiderPressure(
    dbd_server::WorldState& world,
    dbd::Id founder_player_id,
    dbd::Id raider_player_id,
    dbd::Id raider_squad_id,
    dbd::Id founder_depot_structure_id,
    const dbd::Vec3& founder_starter_anchor,
    const dbd::Vec3& contested_mid_anchor,
    const dbd::Vec3& contested_worksite_a,
    const dbd::Vec3& contested_worksite_b) {
    constexpr float kStarterSafeRadius = 42.0f;
    constexpr float kPressureZoneRadius = 26.0f;
    constexpr float kPressureEscortRadius = 18.0f;

    const auto raider_unit_ids = CollectLivingUnits(world, raider_player_id);
    if (raider_unit_ids.empty()) {
        return;
    }

    bool damaged_depot_is_exposed = false;
    const auto depot_it = world.structures.find(founder_depot_structure_id);
    if (depot_it != world.structures.end() && depot_it->second.health < depot_it->second.max_health) {
        int nearby_defenders = 0;
        constexpr float kDepotDefenseRadius = 24.0f;
        for (const auto& [_, unit] : world.units) {
            if (!unit.alive || unit.permanently_dead || unit.owner_player_id != founder_player_id) {
                continue;
            }
            if (DistanceSquared(unit.position, depot_it->second.position) <= (kDepotDefenseRadius * kDepotDefenseRadius)) {
                ++nearby_defenders;
            }
        }
        damaged_depot_is_exposed = nearby_defenders < 2;
    }

    std::optional<dbd::Id> attack_target_unit_id;
    float best_unit_distance_sq = 0.0f;
    bool founder_pressing_zone = damaged_depot_is_exposed;
    bool loot_exposed = false;

    for (const auto& [_, drop] : world.dropped_cargo) {
        if (DistanceSquared(drop.position, founder_starter_anchor) < (kStarterSafeRadius * kStarterSafeRadius)) {
            continue;
        }
        const float mid_distance_sq = DistanceSquared(drop.position, contested_mid_anchor);
        const float worksite_a_distance_sq = DistanceSquared(drop.position, contested_worksite_a);
        const float worksite_b_distance_sq = DistanceSquared(drop.position, contested_worksite_b);
        const float nearest_zone_distance_sq = std::min(mid_distance_sq, std::min(worksite_a_distance_sq, worksite_b_distance_sq));
        if (nearest_zone_distance_sq <= (kPressureZoneRadius * kPressureZoneRadius)) {
            founder_pressing_zone = true;
            loot_exposed = true;
        }
    }

    for (const auto& [unit_id, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || unit.owner_player_id != founder_player_id) {
            continue;
        }

        const float distance_from_starter_sq = DistanceSquared(unit.position, founder_starter_anchor);
        if (distance_from_starter_sq < (kStarterSafeRadius * kStarterSafeRadius)) {
            continue;
        }

        const float mid_distance_sq = DistanceSquared(unit.position, contested_mid_anchor);
        const float worksite_a_distance_sq = DistanceSquared(unit.position, contested_worksite_a);
        const float worksite_b_distance_sq = DistanceSquared(unit.position, contested_worksite_b);
        const float nearest_zone_distance_sq = std::min(mid_distance_sq, std::min(worksite_a_distance_sq, worksite_b_distance_sq));
        if (nearest_zone_distance_sq > (kPressureZoneRadius * kPressureZoneRadius)) {
            continue;
        }

        founder_pressing_zone = true;
        const float cargo_weight = CargoWeight(unit.cargo);
        const bool loaded_hauler = cargo_weight > 0.0f || unit.assignment.route_active;
        const bool overburdened_hauler = cargo_weight > unit.carry_capacity;
        const bool active_mid_route =
            unit.assignment.route_active &&
            (unit.assignment.route_phase == dbd::HaulRoutePhase::ToSource ||
             unit.assignment.route_phase == dbd::HaulRoutePhase::ToStorage);
        const float target_score = nearest_zone_distance_sq - (overburdened_hauler ? 600.0f : (loaded_hauler ? 240.0f : 0.0f));
        if (active_mid_route || overburdened_hauler) {
            founder_pressing_zone = true;
        }
        if (!attack_target_unit_id.has_value() || target_score < best_unit_distance_sq) {
            attack_target_unit_id = unit_id;
            best_unit_distance_sq = target_score;
        }
    }

    std::optional<dbd::Id> attack_target_site_id;
    float best_site_distance_sq = 0.0f;
    for (const auto& [site_id, site] : world.construction_sites) {
        if (site.owner_player_id != founder_player_id ||
            site.stage == dbd::ConstructionStage::Destroyed ||
            site.stage == dbd::ConstructionStage::Completed) {
            continue;
        }
        if (DistanceSquared(site.position, founder_starter_anchor) < (kStarterSafeRadius * kStarterSafeRadius)) {
            continue;
        }

        const float worksite_a_distance_sq = DistanceSquared(site.position, contested_worksite_a);
        const float worksite_b_distance_sq = DistanceSquared(site.position, contested_worksite_b);
        const float nearest_build_line_distance_sq = std::min(worksite_a_distance_sq, worksite_b_distance_sq);
        if (nearest_build_line_distance_sq > (kPressureEscortRadius * kPressureEscortRadius)) {
            continue;
        }

        founder_pressing_zone = true;
        if (!attack_target_site_id.has_value() || nearest_build_line_distance_sq < best_site_distance_sq) {
            attack_target_site_id = site_id;
            best_site_distance_sq = nearest_build_line_distance_sq;
        }
    }

    std::optional<dbd::Id> attack_target_flatten_worker_id;
    float best_flatten_worker_distance_sq = 0.0f;
    for (const auto& [_, job] : world.flatten_jobs) {
        if (job.state == dbd::FlattenJobStateKind::Completed ||
            job.state == dbd::FlattenJobStateKind::Canceled ||
            job.assigned_unit_ids.empty()) {
            continue;
        }
        if (DistanceSquared(job.center, founder_starter_anchor) < (kStarterSafeRadius * kStarterSafeRadius)) {
            continue;
        }
        const float worksite_a_distance_sq = DistanceSquared(job.center, contested_worksite_a);
        const float worksite_b_distance_sq = DistanceSquared(job.center, contested_worksite_b);
        const float nearest_build_line_distance_sq = std::min(worksite_a_distance_sq, worksite_b_distance_sq);
        if (nearest_build_line_distance_sq > (kPressureEscortRadius * kPressureEscortRadius)) {
            continue;
        }

        founder_pressing_zone = true;
        for (dbd::Id unit_id : job.assigned_unit_ids) {
            const auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end() ||
                !unit_it->second.alive ||
                unit_it->second.permanently_dead ||
                unit_it->second.owner_player_id != founder_player_id) {
                continue;
            }
            const float worker_distance_sq = DistanceSquared(unit_it->second.position, job.center);
            if (!attack_target_flatten_worker_id.has_value() || worker_distance_sq < best_flatten_worker_distance_sq) {
                attack_target_flatten_worker_id = unit_id;
                best_flatten_worker_distance_sq = worker_distance_sq;
            }
        }
    }

    if (!founder_pressing_zone) {
        dbd_server::SetSquadStance(world, raider_player_id, raider_squad_id, dbd::SquadStance::Defensive);
        for (dbd::Id unit_id : raider_unit_ids) {
            auto unit_it = world.units.find(unit_id);
            if (unit_it == world.units.end()) {
                continue;
            }
            auto& unit = unit_it->second;
            if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
                unit.current_order = dbd::UnitOrderType::Idle;
                unit.assignment = {};
            }
            unit.move_target = unit.position;
        }
        return;
    }

    dbd_server::SetSquadStance(world, raider_player_id, raider_squad_id, dbd::SquadStance::Aggressive);

    if (damaged_depot_is_exposed) {
        dbd_server::ResolveAttack(
            world,
            raider_player_id,
            raider_unit_ids,
            dbd::AttackTargetKind::Structure,
            founder_depot_structure_id);
        return;
    }

    if (attack_target_unit_id.has_value()) {
        dbd_server::ResolveAttack(
            world,
            raider_player_id,
            raider_unit_ids,
            dbd::AttackTargetKind::Unit,
            *attack_target_unit_id);
        return;
    }

    if (attack_target_flatten_worker_id.has_value()) {
        dbd_server::ResolveAttack(
            world,
            raider_player_id,
            raider_unit_ids,
            dbd::AttackTargetKind::Unit,
            *attack_target_flatten_worker_id);
        return;
    }

    if (attack_target_site_id.has_value()) {
        dbd_server::ResolveAttack(
            world,
            raider_player_id,
            raider_unit_ids,
            dbd::AttackTargetKind::ConstructionSite,
            *attack_target_site_id);
        return;
    }

    if (loot_exposed) {
        for (const auto& [_, drop] : world.dropped_cargo) {
            dbd_server::IssueMoveOrder(world, raider_player_id, raider_unit_ids, drop.position);
            return;
        }
    }

    dbd_server::IssueMoveOrder(world, raider_player_id, raider_unit_ids, contested_mid_anchor);
}

std::string BuildPrimaryRuntimeAlert(
    const dbd_server::WorldState& world,
    dbd::Id primary_player_id,
    dbd::Id primary_depot_structure_id) {
    const auto player_it = world.players.find(primary_player_id);
    if (player_it == world.players.end()) {
        return {};
    }
    const auto structure_it = world.structures.find(primary_depot_structure_id);
    if (structure_it == world.structures.end()) {
        return "Primary depot lost; stored resources were dropped and destroyed in part.";
    }
    for (const auto& [_, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || unit.owner_player_id != primary_player_id) {
            continue;
        }
        const float cargo_weight = CargoWeight(unit.cargo);
        if (cargo_weight > unit.carry_capacity) {
            for (const auto& [_, other] : world.units) {
                if (!other.alive || other.permanently_dead || other.owner_player_id == primary_player_id) {
                    continue;
                }
                if (DistanceSquared(unit.position, other.position) <= (32.0f * 32.0f)) {
                    return "Hauler threatened; overloaded loot carrier is in enemy reach.";
                }
            }
            return "Loot exposed; overloaded hauler needs storage or escort.";
        }
        if (cargo_weight > 0.0f && unit.current_order != dbd::UnitOrderType::HaulToStorage) {
            return "Loot exposed; cargo is not safe until it reaches storage.";
        }
    }
    for (const auto& [_, drop] : world.dropped_cargo) {
        if (DistanceSquared(drop.position, structure_it->second.position) > (24.0f * 24.0f)) {
            return "Loot exposed; dropped cargo can still be recovered or stolen.";
        }
    }
    if (structure_it->second.health < structure_it->second.max_health) {
        return "Primary depot damaged; storage is under pressure.";
    }
    if (player_it->second.pressure_ratio >= 0.35f) {
        return "High economic pressure; shortage penalties are active.";
    }
    if (player_it->second.shortage_ratio > 0.0f) {
        return "Economic pressure rising; shortages are reducing efficiency.";
    }
    return {};
}

std::string BuildSupplyAlert(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    std::size_t active_routes = 0;
    std::size_t threatened_routes = 0;
    std::size_t critical_routes = 0;
    dbd::Id enemy_id = 0;

    for (const auto& [_, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead || unit.owner_player_id != primary_player_id || !unit.assignment.route_active) {
            continue;
        }
        ++active_routes;
        if (unit.route_threat_level == "critical") {
            ++critical_routes;
            if (enemy_id == 0) {
                enemy_id = unit.route_threat_enemy_id;
            }
        } else if (unit.route_threat_level == "threatened") {
            ++threatened_routes;
            if (enemy_id == 0) {
                enemy_id = unit.route_threat_enemy_id;
            }
        }
    }

    if (critical_routes > 0) {
        std::ostringstream out;
        out << "Hauler critical";
        if (enemy_id != 0) {
            out << "; enemy #" << enemy_id << " is on the supply line";
        }
        return out.str();
    }
    if (threatened_routes > 0) {
        std::ostringstream out;
        out << "Supply route threatened: " << threatened_routes << "/" << active_routes << " route hauler(s) exposed";
        if (enemy_id != 0) {
            out << " near enemy #" << enemy_id;
        }
        return out.str();
    }
    if (active_routes > 0) {
        return "Supply route stable.";
    }
    return {};
}

std::string BuildRepairAlert(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    std::size_t damaged_structures = 0;
    std::size_t critical_structures = 0;
    std::size_t damaged_sites = 0;
    bool stalled_repair = false;
    float stored_materials = 0.0f;

    for (const auto& [_, storage] : world.storage_sites) {
        if (storage.owner_player_id != primary_player_id) {
            continue;
        }
        for (const auto& stack : storage.stored_resources) {
            if (stack.category == dbd::CargoCategory::Resource) {
                stored_materials += static_cast<float>(stack.amount);
            }
        }
    }
    for (const auto& [_, structure] : world.structures) {
        if (structure.owner_player_id != primary_player_id || structure.health >= structure.max_health) {
            continue;
        }
        ++damaged_structures;
        if (structure.max_health > 0.0f && structure.health / structure.max_health <= 0.35f) {
            ++critical_structures;
        }
    }
    for (const auto& [_, site] : world.construction_sites) {
        if (site.owner_player_id == primary_player_id && site.health < site.max_health) {
            ++damaged_sites;
        }
    }
    for (const auto& [_, unit] : world.units) {
        if (unit.owner_player_id == primary_player_id && unit.tactical_state == "Repair stalled: no materials") {
            stalled_repair = true;
            break;
        }
    }

    if (stalled_repair) {
        return "Repair stalled: no materials.";
    }
    if (critical_structures > 0) {
        return "Critical structure damaged; repair or defend now.";
    }
    if ((damaged_structures > 0 || damaged_sites > 0) && stored_materials <= 0.0f) {
        return "Depot materials empty; repair needs field supply.";
    }
    if (stored_materials > 0.0f && stored_materials < 12.0f) {
        return "Depot materials low.";
    }
    if (damaged_structures > 0 || damaged_sites > 0) {
        std::ostringstream out;
        out << "Repairable damage: structures " << damaged_structures << ", sites " << damaged_sites << ".";
        return out.str();
    }
    return {};
}

std::string BuildCombatSummary(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    std::size_t friendly_fighting = 0;
    std::size_t enemy_fighting = 0;
    std::size_t friendly_guarding = 0;
    std::size_t friendly_low_hp = 0;
    std::size_t damaged_sites = 0;
    std::size_t damaged_structures = 0;

    for (const auto& [_, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }
        const bool friendly = unit.owner_player_id == primary_player_id;
        if (unit.current_order == dbd::UnitOrderType::AttackTarget) {
            if (friendly) {
                ++friendly_fighting;
            } else {
                ++enemy_fighting;
            }
        }
        if (friendly && unit.current_order == dbd::UnitOrderType::Escort) {
            ++friendly_guarding;
        }
        if (friendly && unit.max_health > 0.0f && unit.health / unit.max_health <= 0.35f) {
            ++friendly_low_hp;
        }
    }

    for (const auto& [_, site] : world.construction_sites) {
        if (site.owner_player_id == primary_player_id && site.health < site.max_health) {
            ++damaged_sites;
        }
    }
    for (const auto& [_, structure] : world.structures) {
        if (structure.owner_player_id == primary_player_id && structure.health < structure.max_health) {
            ++damaged_structures;
        }
    }

    if (friendly_fighting == 0 && enemy_fighting == 0 && friendly_guarding == 0 &&
        friendly_low_hp == 0 && damaged_sites == 0 && damaged_structures == 0) {
        return "No active combat pressure.";
    }

    std::ostringstream out;
    out << "Combat: friendly " << friendly_fighting
        << ", enemy " << enemy_fighting
        << ", guard " << friendly_guarding;
    if (friendly_low_hp > 0) {
        out << ", low HP " << friendly_low_hp;
    }
    if (damaged_sites > 0) {
        out << ", damaged sites " << damaged_sites;
    }
    if (damaged_structures > 0) {
        out << ", damaged structures " << damaged_structures;
    }
    return out.str();
}

std::string BuildCombatAlert(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    const dbd::UnitState* lowest_fighting_unit = nullptr;
    float lowest_fighting_ratio = 1.0f;
    std::size_t enemies_targeting_friendly = 0;
    const dbd::ConstructionSiteState* critical_site = nullptr;
    float critical_site_ratio = 1.0f;
    const dbd::StructureState* critical_structure = nullptr;
    float critical_structure_ratio = 1.0f;

    for (const auto& [_, unit] : world.units) {
        if (!unit.alive || unit.permanently_dead) {
            continue;
        }
        if (unit.owner_player_id == primary_player_id) {
            if (unit.current_order == dbd::UnitOrderType::AttackTarget && unit.max_health > 0.0f) {
                const float ratio = unit.health / unit.max_health;
                if (ratio < lowest_fighting_ratio) {
                    lowest_fighting_ratio = ratio;
                    lowest_fighting_unit = &unit;
                }
            }
            continue;
        }

        if (unit.current_order == dbd::UnitOrderType::AttackTarget &&
            unit.assignment.target_kind == dbd::AttackTargetKind::Unit) {
            const auto target_it = world.units.find(unit.assignment.target_entity_id);
            if (target_it != world.units.end() && target_it->second.owner_player_id == primary_player_id) {
                ++enemies_targeting_friendly;
            }
        }
    }

    for (const auto& [_, site] : world.construction_sites) {
        if (site.owner_player_id != primary_player_id || site.max_health <= 0.0f ||
            site.stage == dbd::ConstructionStage::Destroyed) {
            continue;
        }
        const float ratio = site.health / site.max_health;
        if (ratio < critical_site_ratio) {
            critical_site_ratio = ratio;
            critical_site = &site;
        }
    }

    for (const auto& [_, structure] : world.structures) {
        if (structure.owner_player_id != primary_player_id || structure.max_health <= 0.0f) {
            continue;
        }
        const float ratio = structure.health / structure.max_health;
        if (ratio < critical_structure_ratio) {
            critical_structure_ratio = ratio;
            critical_structure = &structure;
        }
    }

    std::ostringstream out;
    if (lowest_fighting_unit != nullptr && lowest_fighting_ratio <= 0.35f) {
        out << "Retreat unit #" << lowest_fighting_unit->unit_id
            << " now: HP " << static_cast<int>(std::round(lowest_fighting_ratio * 100.0f)) << "%";
        if (enemies_targeting_friendly > 0) {
            out << ", enemies targeting friendlies " << enemies_targeting_friendly;
        }
        return out.str();
    }
    if (critical_site != nullptr && critical_site_ratio <= 0.45f) {
        out << "Construction site #" << critical_site->construction_site_id
            << " critical: HP " << static_cast<int>(std::round(critical_site_ratio * 100.0f)) << "%";
        return out.str();
    }
    if (critical_structure != nullptr && critical_structure_ratio <= 0.45f) {
        out << "Structure #" << critical_structure->structure_id
            << " critical: HP " << static_cast<int>(std::round(critical_structure_ratio * 100.0f)) << "%";
        return out.str();
    }
    if (enemies_targeting_friendly > 0) {
        out << "Enemy pressure: " << enemies_targeting_friendly << " attacker(s) targeting friendly units";
        return out.str();
    }
    return {};
}

std::string BuildContactSummary(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    std::size_t visible = 0;
    std::size_t stale = 0;
    const auto contacts_it = world.known_contacts.find(primary_player_id);
    if (contacts_it != world.known_contacts.end()) {
        for (const auto& [_, contact] : contacts_it->second) {
            if (contact.currently_visible) {
                ++visible;
            } else {
                ++stale;
            }
        }
    }

    std::ostringstream out;
    out << "Contacts: visible " << visible << ", stale " << stale;
    return out.str();
}

std::string BuildContactAlert(const dbd_server::WorldState& world, dbd::Id primary_player_id) {
    const auto contacts_it = world.known_contacts.find(primary_player_id);
    if (contacts_it == world.known_contacts.end() || contacts_it->second.empty()) {
        return {};
    }

    const dbd::KnownContactState* newest_visible = nullptr;
    const dbd::KnownContactState* newest_stale = nullptr;
    for (const auto& [_, contact] : contacts_it->second) {
        if (contact.currently_visible) {
            if (newest_visible == nullptr || contact.last_seen_tick > newest_visible->last_seen_tick) {
                newest_visible = &contact;
            }
        } else if (newest_stale == nullptr || contact.last_seen_tick > newest_stale->last_seen_tick) {
            newest_stale = &contact;
        }
    }

    std::ostringstream out;
    if (newest_visible != nullptr) {
        out << "New contact #" << newest_visible->target_entity_id << " visible";
        if (newest_visible->spotted_by_unit_id != 0) {
            out << " by scout #" << newest_visible->spotted_by_unit_id;
        }
        return out.str();
    }
    if (newest_stale != nullptr) {
        out << "Contact stale #" << newest_stale->target_entity_id << " last seen";
        if (newest_stale->spotted_by_unit_id != 0) {
            out << " by #" << newest_stale->spotted_by_unit_id;
        }
        return out.str();
    }
    return {};
}

void WriteSessionStatus(
    const std::filesystem::path& output_path,
    const dbd_server::WorldState& world,
    dbd::Id primary_player_id,
    dbd::Id primary_depot_structure_id,
    std::uint64_t tick,
    const std::string& mode,
    const std::filesystem::path& world_save_root,
    const std::filesystem::path& command_spool_path,
    const std::string& last_command_message,
    bool last_command_ok) {
    const auto temp_path = TempOutputPath(output_path);
    std::ofstream out(temp_path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    out << "{\n";
    out << "  \"tick\": " << tick << ",\n";
    out << "  \"mode\": \"" << dbd_server::CommandSpoolJsonEscape(mode) << "\",\n";
    out << "  \"worldSaveRoot\": \"" << dbd_server::CommandSpoolJsonEscape(world_save_root.string()) << "\",\n";
    out << "  \"commandSpoolPath\": \"" << dbd_server::CommandSpoolJsonEscape(command_spool_path.string()) << "\",\n";
    out << "  \"lastCommandOk\": " << (last_command_ok ? "true" : "false") << ",\n";
    out << "  \"lastCommandMessage\": \"" << dbd_server::CommandSpoolJsonEscape(last_command_message) << "\"";
    out << ",\n";
    out << "  \"combatSummary\": \"" << dbd_server::CommandSpoolJsonEscape(BuildCombatSummary(world, primary_player_id)) << "\"";
    const auto combat_alert = BuildCombatAlert(world, primary_player_id);
    if (!combat_alert.empty()) {
        out << ",\n";
        out << "  \"combatAlert\": \"" << dbd_server::CommandSpoolJsonEscape(combat_alert) << "\"";
    }
    out << ",\n";
    out << "  \"contactSummary\": \"" << dbd_server::CommandSpoolJsonEscape(BuildContactSummary(world, primary_player_id)) << "\"";
    const auto contact_alert = BuildContactAlert(world, primary_player_id);
    if (!contact_alert.empty()) {
        out << ",\n";
        out << "  \"contactAlert\": \"" << dbd_server::CommandSpoolJsonEscape(contact_alert) << "\"";
    }
    const auto depot_run = BuildDepotRunObjective(world, primary_player_id, primary_depot_structure_id, tick);
    out << ",\n";
    out << "  \"scenario\": {\n";
    out << "    \"name\": \"" << dbd_server::CommandSpoolJsonEscape(depot_run.name) << "\",\n";
    out << "    \"state\": \"" << dbd_server::CommandSpoolJsonEscape(depot_run.state) << "\",\n";
    out << "    \"reason\": \"" << dbd_server::CommandSpoolJsonEscape(depot_run.reason) << "\",\n";
    out << "    \"summary\": \"" << dbd_server::CommandSpoolJsonEscape(depot_run.summary) << "\",\n";
    out << "    \"ironFittingStored\": " << depot_run.iron_fitting_stored << ",\n";
    out << "    \"ironFittingGoal\": " << depot_run.iron_fitting_goal << ",\n";
    out << "    \"ironNodeId\": " << depot_run.iron_node_id << ",\n";
    out << "    \"ironNodePosition\": {\"x\": " << depot_run.iron_node_position.x
        << ", \"y\": " << depot_run.iron_node_position.y
        << ", \"z\": " << depot_run.iron_node_position.z << "},\n";
    out << "    \"forwardDepotComplete\": " << (depot_run.forward_depot_complete ? "true" : "false") << ",\n";
    out << "    \"livingUnits\": " << depot_run.living_units << ",\n";
    out << "    \"unitsLost\": " << depot_run.units_lost << ",\n";
    out << "    \"primaryDepotHealth\": " << depot_run.primary_depot_health << ",\n";
    out << "    \"primaryDepotMaxHealth\": " << depot_run.primary_depot_max_health << ",\n";
    out << "    \"timeLimitTicks\": " << depot_run.time_limit_ticks << ",\n";
    out << "    \"remainingTicks\": " << depot_run.remaining_ticks << "\n";
    out << "  }";
    const auto player_it = world.players.find(primary_player_id);
    if (player_it != world.players.end()) {
        const auto& player = player_it->second;
        const char* pressure_band = "stable";
        if (player.pressure_ratio >= 0.35f) {
            pressure_band = "high";
        } else if (player.pressure_ratio >= 0.15f) {
            pressure_band = "medium";
        }
        out << ",\n";
        out << "  \"economy\": {\n";
        out << "    \"playerId\": " << primary_player_id << ",\n";
        out << "    \"credits\": " << player.credits << ",\n";
        out << "    \"taxLoad\": " << player.tax_load << ",\n";
        out << "    \"upkeepLoad\": " << player.upkeep_load << ",\n";
        out << "    \"creditLoad\": " << player.credit_load << ",\n";
        out << "    \"complexityLoad\": " << player.complexity_load << ",\n";
        out << "    \"foodLoad\": " << player.food_load << ",\n";
        out << "    \"foodShortage\": " << player.food_shortage_ratio << ",\n";
        out << "    \"upkeepShortage\": " << player.upkeep_shortage_ratio << ",\n";
        out << "    \"shortageRatio\": " << player.shortage_ratio << ",\n";
        out << "    \"efficiency\": " << player.efficiency << ",\n";
        out << "    \"pressureRatio\": " << player.pressure_ratio << ",\n";
        out << "    \"pressureBand\": \"" << pressure_band << "\"\n";
        out << "  }\n";
    } else {
        out << "\n";
    }
    const auto runtime_alert = BuildPrimaryRuntimeAlert(world, primary_player_id, primary_depot_structure_id);
    if (!runtime_alert.empty()) {
        out << ",\n";
        out << "  \"runtimeAlert\": \"" << dbd_server::CommandSpoolJsonEscape(runtime_alert) << "\"\n";
    }
    const auto supply_alert = BuildSupplyAlert(world, primary_player_id);
    if (!supply_alert.empty()) {
        out << ",\n";
        out << "  \"supplyAlert\": \"" << dbd_server::CommandSpoolJsonEscape(supply_alert) << "\"\n";
    }
    const auto repair_alert = BuildRepairAlert(world, primary_player_id);
    if (!repair_alert.empty()) {
        out << ",\n";
        out << "  \"repairAlert\": \"" << dbd_server::CommandSpoolJsonEscape(repair_alert) << "\"\n";
    }
    out << "}\n";
    out.close();
    if (!out) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        return;
    }
    CommitTempOutputFile(temp_path, output_path);
}

void WriteSessionManifest(
    const dbd_server::WorldState& world,
    const std::filesystem::path& output_path,
    dbd::Id founder_player_id,
    dbd::Id raider_player_id) {
    const auto temp_path = TempOutputPath(output_path);
    std::ofstream out(temp_path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    out << "{\n";
    out << "  \"players\": [\n";
    bool first = true;
    for (const auto& [player_id, player] : world.players) {
        if (!first) {
            out << ",\n";
        }
        first = false;
        out << "    {\"playerId\": " << player_id
            << ", \"name\": \"" << dbd_server::CommandSpoolJsonEscape(player.display_name) << "\""
            << ", \"isPrimary\": " << (player_id == founder_player_id ? "true" : "false")
            << ", \"isRaider\": " << (player_id == raider_player_id ? "true" : "false") << "}";
    }
    out << "\n  ]\n";
    out << "}\n";
    out.close();
    if (!out) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        return;
    }
    CommitTempOutputFile(temp_path, output_path);
}

dbd_server::CommandResult ApplyCommand(dbd_server::WorldState& world, const SessionCommand& command) {
    using dbd_server::CommandResult;
    const dbd::Id controller_player_id = command.controller_player_id != 0 ? command.controller_player_id : command.player_id;
    const dbd::Id player_id = command.player_id != 0 ? command.player_id : controller_player_id;

    if (command.type == "debug_destroy_primary_depot") {
        for (auto it = world.structures.begin(); it != world.structures.end(); ++it) {
            if (it->second.owner_player_id == player_id &&
                it->second.structure_type == dbd::StructureType::StorageDepot) {
                const dbd::Id structure_id = it->first;
                world.structures.erase(it);
                return {true, "Debug: primary depot destroyed.", structure_id};
            }
        }
        return {false, "Debug failed: primary depot not found."};
    }
    if (command.type == "debug_kill_founder_units") {
        std::size_t killed = 0;
        for (auto& [_, unit] : world.units) {
            if (unit.owner_player_id != player_id || !unit.alive || unit.permanently_dead) {
                continue;
            }
            unit.health = 0.0f;
            unit.alive = false;
            unit.permanently_dead = true;
            unit.current_order = dbd::UnitOrderType::Idle;
            unit.queued_orders.clear();
            ++killed;
        }
        return killed > 0
            ? CommandResult {true, "Debug: founder units killed.", static_cast<dbd::Id>(killed)}
            : CommandResult {false, "Debug failed: no living founder units found."};
    }
    if (command.type == "debug_expire_scenario_timer") {
        world.tick = std::max<std::uint64_t>(world.tick, kDepotRunTimeLimitTicks);
        return {true, "Debug: Depot Run timer expired.", static_cast<dbd::Id>(world.tick)};
    }
    if (command.type == "move") {
        return dbd_server::IssueMoveOrder(world, controller_player_id, command.unit_ids, command.target_position);
    }
    if (command.type == "formation_move") {
        return dbd_server::IssueFormationMoveOrder(world, controller_player_id, command.unit_ids, command.target_position);
    }
    if (command.type == "queue_move") {
        return dbd_server::QueueMoveOrder(world, controller_player_id, command.unit_ids, command.target_position);
    }
    if (command.type == "clear_queue") {
        return dbd_server::ClearQueuedOrders(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "attack") {
        return dbd_server::ResolveAttack(world, controller_player_id, command.unit_ids, command.target_kind, command.target_entity_id);
    }
    if (command.type == "focus_fire") {
        return dbd_server::IssueFocusFireOrder(world, controller_player_id, command.unit_ids, command.target_kind, command.target_entity_id);
    }
    if (command.type == "retreat_selected") {
        return dbd_server::IssueRetreatOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "intercept_unit") {
        return dbd_server::IssueInterceptUnitOrder(world, controller_player_id, command.unit_ids, command.target_entity_id);
    }
    if (command.type == "scout_area") {
        return dbd_server::IssueScoutAreaOrder(world, controller_player_id, command.unit_ids, command.center, command.radius <= 0.0f ? 18.0f : command.radius);
    }
    if (command.type == "queue_scout_area") {
        return dbd_server::QueueScoutAreaOrder(world, controller_player_id, command.unit_ids, command.center, command.radius <= 0.0f ? 18.0f : command.radius);
    }
    if (command.type == "patrol_route") {
        return dbd_server::IssuePatrolRouteOrder(world, controller_player_id, command.unit_ids, command.point_a, command.point_b);
    }
    if (command.type == "queue_patrol_route") {
        return dbd_server::QueuePatrolRouteOrder(world, controller_player_id, command.unit_ids, command.point_a, command.point_b);
    }
    if (command.type == "investigate_contact") {
        return dbd_server::IssueInvestigateContactOrder(world, controller_player_id, command.unit_ids, command.target_kind, command.target_entity_id);
    }
    if (command.type == "guard_unit") {
        return dbd_server::IssueGuardUnitOrder(world, controller_player_id, command.unit_ids, command.target_entity_id);
    }
    if (command.type == "guard_site") {
        return dbd_server::IssueGuardSiteOrder(world, controller_player_id, command.unit_ids, command.target_kind, command.target_entity_id);
    }
    if (command.type == "hold_position") {
        return dbd_server::IssueHoldPositionOrder(world, controller_player_id, command.unit_ids, command.target_position);
    }
    if (command.type == "stop") {
        return dbd_server::IssueStopOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "emergency_deposit") {
        return dbd_server::IssueEmergencyDepositOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "drop_cargo") {
        return dbd_server::IssueDropCargoOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "guard_threatened_route") {
        return dbd_server::IssueGuardThreatenedRouteOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "intercept_route_threat") {
        return dbd_server::IssueInterceptRouteThreatOrder(world, controller_player_id, command.unit_ids);
    }
    if (command.type == "repair_structure") {
        return dbd_server::IssueRepairStructureOrder(world, controller_player_id, command.unit_ids, command.target_entity_id);
    }
    if (command.type == "repair_construction_site") {
        return dbd_server::IssueRepairConstructionSiteOrder(world, controller_player_id, command.unit_ids, command.target_entity_id);
    }
    if (command.type == "supply_repair") {
        return dbd_server::IssueSupplyRepairOrder(world, controller_player_id, command.unit_ids, command.target_kind, command.target_entity_id);
    }
    if (command.type == "craft_item") {
        return dbd_server::CraftItem(world, player_id, command.item_id, command.amount);
    }
    if (command.type == "install_item") {
        return dbd_server::InstallItem(world, controller_player_id, command.item_id, command.position);
    }
    if (command.type == "use_item") {
        return dbd_server::UseItem(world, controller_player_id, command.unit_ids, command.item_id);
    }
    if (command.type == "harvest") {
        return dbd_server::IssueHarvestOrder(world, controller_player_id, command.unit_id, command.resource_node_id);
    }
    if (command.type == "loot") {
        if (command.unit_id != 0) {
            return dbd_server::IssueLootOrder(world, controller_player_id, command.unit_id, command.dropped_cargo_id);
        }
        if (command.unit_ids.empty()) {
            return {false, "Loot rejected: unitId or unitIds is required."};
        }
        bool all_ok = true;
        std::ostringstream message;
        for (std::size_t i = 0; i < command.unit_ids.size(); ++i) {
            const auto result = dbd_server::IssueLootOrder(world, controller_player_id, command.unit_ids[i], command.dropped_cargo_id);
            if (i != 0) {
                message << " | ";
            }
            message << "#" << command.unit_ids[i] << ": " << result.message;
            all_ok = all_ok && result.ok;
            if (world.dropped_cargo.find(command.dropped_cargo_id) == world.dropped_cargo.end()) {
                break;
            }
        }
        return {all_ok, message.str(), command.dropped_cargo_id};
    }
    if (command.type == "haul_route") {
        return dbd_server::IssueHaulRouteOrder(
            world,
            controller_player_id,
            command.unit_ids,
            command.source_kind,
            command.source_id,
            command.storage_site_id);
    }
    if (command.type == "return_to_storage") {
        return dbd_server::IssueReturnToStorageOrder(
            world,
            controller_player_id,
            command.unit_id,
            command.storage_site_id);
    }
    if (command.type == "start_flatten") {
        const auto start = dbd_server::StartFlattenJob(world, player_id, command.center, command.radius, command.target_grade);
        if (!start.ok || command.unit_ids.empty()) {
            return start;
        }
        const auto assign = dbd_server::AssignUnitsToFlattenJob(world, controller_player_id, start.entity_id, command.unit_ids);
        if (!assign.ok) {
            world.flatten_jobs.erase(start.entity_id);
            return assign;
        }
        return assign.ok ? CommandResult {true, "Flatten job created and units assigned.", start.entity_id} : assign;
    }
    if (command.type == "start_construction") {
        if (player_id != controller_player_id) {
            return {false, "Construction rejected: controller must match the owning player."};
        }
        const auto start = dbd_server::StartConstruction(
            world,
            player_id,
            command.structure_type,
            command.position,
            command.footprint_radius);
        return start;
    }
    if (command.type == "set_unit_automation") {
        if (command.unit_id != 0) {
            return dbd_server::SetUnitAutomationRules(
                world,
                controller_player_id,
                command.unit_id,
                command.automation_rules);
        }
        if (command.unit_ids.empty()) {
            return {false, "Unit automation rejected: unitId or unitIds is required."};
        }
        bool all_ok = true;
        std::ostringstream message;
        for (std::size_t i = 0; i < command.unit_ids.size(); ++i) {
            const auto result = dbd_server::SetUnitAutomationRules(
                world,
                controller_player_id,
                command.unit_ids[i],
                command.automation_rules);
            if (i != 0) {
                message << " | ";
            }
            message << "#" << command.unit_ids[i] << ": " << result.message;
            all_ok = all_ok && result.ok;
        }
        return {all_ok, message.str()};
    }
    if (command.type == "set_squad_automation") {
        if (command.squad_id == 0) {
            return {false, "Squad automation rejected: squadId is required."};
        }
        if (player_id != controller_player_id) {
            return {false, "Squad automation rejected: controller must match the owning player."};
        }
        return dbd_server::SetSquadAutomationRules(
            world,
            player_id,
            command.squad_id,
            command.automation_rules);
    }

    return {false, "Unsupported command type: " + command.type};
}

}  // namespace

int main() {
    std::cout << "DBD play session booting..." << std::endl;

    dbd_server::WorldState world;
    dbd_server::InitializeExperimentalChunkGrid(world);
    const auto founder = dbd_server::BootstrapSinglePlayerWarSlice(world, "FoundingPlayer");
    const auto raider = dbd_server::BootstrapSinglePlayerWarSlice(world, "RaiderPlayer");
    dbd_server::SetSquadStance(world, founder.player_id, founder.squad_id, dbd::SquadStance::Defensive);
    dbd_server::SetSquadStance(world, raider.player_id, raider.squad_id, dbd::SquadStance::Defensive);

    const dbd::Vec3 founderStaging {18.0f, 0.0f, 8.0f};
    const dbd::Vec3 raiderStaging {98.0f, 0.0f, 48.0f};
    const dbd::Vec3 contestedMidAnchor {88.0f, 0.0f, 46.0f};
    const dbd::Vec3 contestedWorksiteA {102.0f, 0.0f, 58.0f};
    const dbd::Vec3 contestedWorksiteB {114.0f, 0.0f, 64.0f};

    for (std::size_t i = 0; i < founder.starter_unit_ids.size(); ++i) {
        auto& unit = world.units.at(founder.starter_unit_ids[i]);
        unit.position = {founderStaging.x + static_cast<float>(i * 2), 0.0f, founderStaging.z + static_cast<float>((i % 2) * 2)};
        unit.move_target = unit.position;
    }
    for (std::size_t i = 0; i < raider.starter_unit_ids.size(); ++i) {
        auto& unit = world.units.at(raider.starter_unit_ids[i]);
        unit.position = {raiderStaging.x + static_cast<float>(i * 2), 0.0f, raiderStaging.z + static_cast<float>((i % 2) * 2)};
        unit.move_target = unit.position;
    }

    if (world.structures.find(founder.depot_structure_id) != world.structures.end()) {
        world.structures.at(founder.depot_structure_id).position = {8.0f, 0.0f, 4.0f};
    }
    if (world.storage_sites.find(founder.depot_storage_id) != world.storage_sites.end()) {
        world.storage_sites.at(founder.depot_storage_id).position = {8.0f, 0.0f, 4.0f};
        world.storage_sites.at(founder.depot_storage_id).stored_resources.push_back(
            dbd::CargoStack {91'001, 30, 180.0f, 1.0f, dbd::CargoCategory::Resource});
        world.storage_sites.at(founder.depot_storage_id).stored_resources.push_back(
            dbd::CargoStack {91'002, 12, 120.0f, 2.4f, dbd::CargoCategory::Resource});
        world.storage_sites.at(founder.depot_storage_id).stored_resources.push_back(
            dbd::CargoStack {91'003, 8, 144.0f, 1.2f, dbd::CargoCategory::Resource});
    }
    if (world.structures.find(raider.depot_structure_id) != world.structures.end()) {
        world.structures.at(raider.depot_structure_id).position = {122.0f, 0.0f, 56.0f};
    }
    if (world.storage_sites.find(raider.depot_storage_id) != world.storage_sites.end()) {
        world.storage_sites.at(raider.depot_storage_id).position = {122.0f, 0.0f, 56.0f};
    }

    if (founder.resource_node_ids.size() > 1) {
        const auto mid_node_id = founder.resource_node_ids[1];
        if (world.resource_nodes.find(mid_node_id) != world.resource_nodes.end()) {
            world.resource_nodes.at(mid_node_id).position = contestedMidAnchor;
        }
    }

    dbd_server::StartFlattenJob(world, founder.player_id, contestedWorksiteA, 8.0f, 0.14f);
    dbd_server::StartFlattenJob(world, founder.player_id, contestedWorksiteB, 8.0f, 0.14f);

    const auto save_root = std::filesystem::path(DBD_REBOOT_ROOT) / "runtime-save";
    std::filesystem::create_directories(save_root);
    const auto snapshot_path = save_root / "world_snapshot.json";
    const auto status_path = save_root / "session_status.json";
    const auto manifest_path = save_root / "session_manifest.json";
    const auto report_path = save_root / "regression_report.json";
    const auto command_spool = dbd_server::PrepareCommandSpool(save_root);

    WriteSessionManifest(world, manifest_path, founder.player_id, raider.player_id);
    {
        std::ofstream report(report_path, std::ios::trunc);
        report << "{\n";
        report << "  \"overallPass\": true,\n";
        report << "  \"steps\": [],\n";
        report << "  \"mode\": \"play_session\",\n";
        report << "  \"worldSaveRoot\": \"" << dbd_server::CommandSpoolJsonEscape(save_root.string()) << "\",\n";
        report << "  \"commandSpoolPath\": \"" << dbd_server::CommandSpoolJsonEscape(command_spool.root.string()) << "\"\n";
        report << "}\n";
    }

    std::string last_command_message = "Free-play session started. Starter nodes are safe but low yield; mid-field and frontier nodes pay more inside raider pressure range.";
    bool last_command_ok = true;
    constexpr float kTickSeconds = 0.1f;
    constexpr int kSnapshotEveryTicks = 1;
    constexpr int kRaiderPressureEveryTicks = 20;
    constexpr int kEconomicSettlementEveryTicks = 100;
    std::uint64_t local_tick = 0;

    dbd_server::RefreshKnownContacts(world);
    dbd_server::RefreshEconomicTelemetry(world);
    dbd_server::RefreshRouteThreats(world);
    dbd_server::RefreshChunkStreaming(world);
    dbd_server::ExportWorldSnapshotJson(world, snapshot_path);
    WriteSessionStatus(
        status_path,
        world,
        founder.player_id,
        founder.depot_structure_id,
        world.tick,
        "play_session",
        save_root,
        command_spool.root,
        last_command_message,
        last_command_ok);
    dbd_server::SaveWorldState(world, save_root);

    while (true) {
        for (const auto& inbox_path : dbd_server::ListCommandInboxFiles(command_spool.inbox)) {
            const auto claimed = dbd_server::ClaimCommandFile(inbox_path, command_spool);
            if (!claimed.has_value()) {
                continue;
            }

            dbd_server::WriteCommandReceipt(*claimed, "processing", "Command claimed for execution.", world.tick);

            const auto commands = ParseCommandPayload(claimed->payload);
            if (commands.empty()) {
                last_command_ok = false;
                last_command_message = "Command file found, but no valid commands were parsed.";
                dbd_server::WriteCommandReceipt(*claimed, "invalid", last_command_message, world.tick);
                dbd_server::WriteCommandResult(*claimed, false, last_command_message, world.tick);
                dbd_server::ArchiveClaimedCommandFile(*claimed, command_spool);
                continue;
            }

            last_command_ok = true;
            std::ostringstream aggregate;
            std::vector<std::pair<std::string, std::string>> command_results;
            for (const auto& command : commands) {
                const auto result = ApplyCommand(world, command);
                if (!aggregate.str().empty()) {
                    aggregate << " | ";
                }
                aggregate << command.type << ": " << result.message;
                const std::string per_command_id = command.command_id.empty() ? command.type : command.command_id;
                command_results.push_back({
                    per_command_id,
                    std::string(result.ok ? "ok: " : "failed: ") + result.message
                });
                last_command_ok = last_command_ok && result.ok;
            }
            last_command_message = aggregate.str();
            dbd_server::WriteCommandReceipt(
                *claimed,
                last_command_ok ? "completed" : "failed",
                last_command_message,
                world.tick);
            dbd_server::WriteCommandBatchResult(*claimed, last_command_ok, last_command_message, world.tick, command_results);
            dbd_server::ArchiveClaimedCommandFile(*claimed, command_spool);
        }

        dbd_server::AdvanceWorldTime(world, static_cast<std::uint64_t>(kTickSeconds * 1000.0f));
        dbd_server::RefreshKnownContacts(world);
        dbd_server::EvaluateAutomation(world, kTickSeconds);
        dbd_server::ProgressGuardOrders(world, kTickSeconds);
        dbd_server::ProgressUnitMovement(world, kTickSeconds);
        dbd_server::ProgressHaulRoutes(world, kTickSeconds);
        dbd_server::ProgressRepairOrders(world, kTickSeconds);
        dbd_server::ProgressCombat(world, kTickSeconds);
        dbd_server::ProgressFlattenJobs(world, kTickSeconds);
        dbd_server::ProgressConstructionSites(world, kTickSeconds);
        dbd_server::RefreshRouteThreats(world);
        dbd_server::RefreshChunkStreaming(world);

        ++local_tick;
        if (local_tick % kRaiderPressureEveryTicks == 0) {
            ApplyMidfieldRaiderPressure(
                world,
                founder.player_id,
                raider.player_id,
                raider.squad_id,
                founder.depot_structure_id,
                founderStaging,
                contestedMidAnchor,
                contestedWorksiteA,
                contestedWorksiteB);
        }
        if (local_tick % kEconomicSettlementEveryTicks == 0) {
            dbd_server::ProcessEconomicSettlement(world, 10.0f);
        }

        if (local_tick % kSnapshotEveryTicks == 0) {
            dbd_server::RefreshEconomicTelemetry(world);
            dbd_server::RefreshRouteThreats(world);
            dbd_server::ExportWorldSnapshotJson(world, snapshot_path);
            WriteSessionStatus(
                status_path,
                world,
                founder.player_id,
                founder.depot_structure_id,
                world.tick,
                "play_session",
                save_root,
                command_spool.root,
                last_command_message,
                last_command_ok);
            dbd_server::SaveWorldState(world, save_root);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(kTickSeconds * 1000.0f)));
    }
}
