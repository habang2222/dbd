#include "dbd/domain_types.hpp"

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <chrono>
#include <vector>

namespace {

using dbd::Id;

struct PlayerView {
    Id player_id {};
    std::string name {};
    bool is_primary {};
    bool is_raider {};
    float credits {};
    float tax_load {};
    float upkeep_load {};
    float credit_load {};
    float complexity_load {};
    float food_load {};
    float food_shortage {};
    float upkeep_shortage {};
    float shortage_ratio {};
    float efficiency {1.0f};
    float pressure_ratio {};
};

struct UnitView {
    Id unit_id {};
    Id owner_player_id {};
    Id controller_player_id {};
    Id squad_id {};
    dbd::Vec3 position {};
    dbd::Vec3 move_target {};
    std::string order {};
    std::string combat_band {"Mid"};
    float health {};
    float max_health {};
    float stamina {};
    float max_stamina {};
    float cargo_weight {};
    Id cargo_primary_item_id {};
    std::string cargo_primary_item_name {"Empty"};
    float carry_capacity {40.0f};
    float vision_range {};
    bool alive {true};
    int assignment_kind {};
    Id assignment_target_entity_id {};
    dbd::Vec3 assignment_target_position {};
    dbd::Vec3 assignment_secondary_position {};
    float assignment_radius {};
    bool assignment_patrol_route {};
    int queued_order_count {};
    std::string next_queued_order {"None"};
    dbd::Vec3 next_queued_target {};
    std::string tactical_state {};
    std::string queue_interrupt_reason {};
    bool route_active {};
    std::string route_source_kind {"None"};
    Id route_source_id {};
    Id route_storage_id {};
    std::string route_phase {"None"};
    std::string route_threat_level {"safe"};
    Id route_threat_enemy_id {};
    std::vector<dbd::AutomationRule> automation_rules {};
};

struct SquadView {
    Id squad_id {};
    Id owner_player_id {};
    Id commander_player_id {};
    Id leader_unit_id {};
    std::string name {};
    std::string stance {};
    std::vector<Id> unit_ids {};
    std::vector<dbd::AutomationRule> automation_rules {};
};

struct FlattenJobView {
    Id flatten_job_id {};
    dbd::Vec3 center {};
    float radius {};
    float required_labor {};
    float accumulated_labor {};
    std::size_t assigned_count {};
    std::string state {};
};

struct ConstructionSiteView {
    Id construction_site_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    float footprint_radius {};
    float health {};
    float max_health {};
    float completion_ratio {};
    bool auto_build {true};
    std::string stage {};
    std::string structure_type {};
};

struct StructureView {
    Id structure_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    float health {};
    float max_health {};
    std::string structure_type {};
};

struct StorageSiteView {
    Id storage_site_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    std::size_t stored_stack_count {};
    Id primary_item_id {};
    std::string primary_item_name {"Empty"};
};

struct ResourceNodeView {
    Id resource_node_id {};
    dbd::Vec3 position {};
    std::string risk_band {"Low"};
    Id produces_item_id {};
    std::string produces_item_name {"Unknown resource"};
    float richness {1.0f};
    float extraction_rate {1.0f};
    std::uint32_t remaining_amount {};
    std::uint32_t max_amount {};
};

struct DroppedCargoView {
    Id dropped_cargo_id {};
    Id source_unit_id {};
    Id primary_item_id {};
    std::string primary_item_name {"Unknown cargo"};
    dbd::Vec3 position {};
    std::size_t stack_count {};
    float total_weight {};
    float total_value {};
};

struct KnownContactView {
    Id observing_player_id {};
    Id target_entity_id {};
    std::string target_kind {};
    dbd::Vec3 position {};
    std::uint64_t last_seen_tick {};
    Id spotted_by_unit_id {};
    std::string freshness {};
};

struct RuntimeSummary {
    std::size_t working_units {};
    std::size_t combat_units {};
    std::size_t hauling_units {};
    std::size_t retreating_units {};
};

struct EconomyStatusView {
    Id player_id {};
    float credits {};
    float tax_load {};
    float upkeep_load {};
    float credit_load {};
    float complexity_load {};
    float food_load {};
    float food_shortage {};
    float upkeep_shortage {};
    float shortage_ratio {};
    float efficiency {1.0f};
    float pressure_ratio {};
    std::string pressure_band {"stable"};
};

struct ScenarioStatusView {
    std::string name {};
    std::string state {"active"};
    std::string reason {"in_progress"};
    std::string summary {};
    float iron_fitting_stored {};
    float iron_fitting_goal {15.0f};
    Id iron_node_id {};
    dbd::Vec3 iron_node_position {};
    bool forward_depot_complete {};
    std::size_t living_units {};
    std::size_t units_lost {};
    float primary_depot_health {};
    float primary_depot_max_health {};
    std::uint64_t time_limit_ticks {1200};
    std::uint64_t remaining_ticks {1200};
};

struct SessionStatusView {
    std::uint64_t tick {};
    std::string mode {};
    bool last_command_ok {};
    std::string last_command_message {};
    EconomyStatusView economy {};
    bool has_economy {};
    std::string runtime_alert {};
    std::string supply_alert {};
    std::string combat_summary {};
    std::string combat_alert {};
    std::string contact_summary {};
    std::string contact_alert {};
    std::string repair_alert {};
    ScenarioStatusView scenario {};
    bool has_scenario {};
};

struct CommandFeedbackView {
    std::string command_id {};
    std::string status {};
    std::string message {};
    std::filesystem::file_time_type updated_at {};
    bool loaded {};
};

struct WorldSnapshotView {
    std::vector<PlayerView> players {};
    std::vector<SquadView> squads {};
    std::vector<UnitView> units {};
    std::vector<FlattenJobView> flatten_jobs {};
    std::vector<ConstructionSiteView> construction_sites {};
    std::vector<StructureView> structures {};
    std::vector<StorageSiteView> storage_sites {};
    std::vector<ResourceNodeView> resource_nodes {};
    std::vector<DroppedCargoView> dropped_cargo {};
    std::vector<KnownContactView> known_contacts {};
};

enum class CommandMode {
    Move,
    Attack,
    Intercept,
    Harvest,
    Flatten,
    Build,
    Loot,
    HaulRoute,
    Guard,
    Repair,
    RepairSupply,
    ScoutArea,
    PatrolRoute,
    InvestigateContact
};

struct CameraState {
    float center_x {64.0f};
    float center_z {48.0f};
    float zoom {5.0f};
};

enum class AutomationScope {
    SelectedUnits,
    ActiveGroup,
    Squad
};

enum class AutomationPreset {
    Gather,
    Escort,
    Scout,
    Hold
};

struct AutomationEditorState {
    AutomationScope scope {AutomationScope::SelectedUnits};
    Id target_squad_id {};
    std::vector<dbd::AutomationRule> working_rules {};
    std::size_t selected_rule_index {};
    std::string source_label {"Selected units"};
    bool dirty {};
    bool mixed_source_rules {};
};

struct AppState {
    std::filesystem::path root_dir {};
    std::filesystem::path runtime_dir {};
    WorldSnapshotView snapshot {};
    SessionStatusView session {};
    CommandFeedbackView command_feedback {};
    std::vector<Id> selected_units {};
    std::array<std::vector<Id>, 4> control_groups {};
    int active_group_slot {-1};
    CameraState camera {};
    CommandMode command_mode {CommandMode::Move};
    dbd::StructureType pending_structure_type {dbd::StructureType::StorageDepot};
    float pending_flatten_radius {8.0f};
    float pending_scout_radius {18.0f};
    bool pending_patrol_start_set {};
    dbd::Vec3 pending_patrol_start {};
    bool pending_haul_source_set {};
    std::string pending_haul_source_kind {"None"};
    Id pending_haul_source_id {};
    bool queue_next_order {};
    int selected_contact_index {-1};
    Id primary_player_id {};
    bool snapshot_loaded {};
    bool dragging_selection {};
    bool panning_camera {};
    POINT drag_start_screen {};
    POINT drag_end_screen {};
    POINT pan_anchor_screen {};
    CameraState pan_anchor_camera {};
    int last_recalled_group_slot {-1};
    std::chrono::steady_clock::time_point last_group_recall_time {};
    AutomationEditorState automation_editor {};
};

AppState* g_app = nullptr;

constexpr wchar_t kWindowClassName[] = L"DBDRebootClientWindow";
constexpr int kHeaderHeight = 88;
constexpr int kPanelWidth = 320;
constexpr int kRefreshIntervalMs = 250;
constexpr Id kIronFittingItemId = 91'003;
constexpr auto kControlGroupDoubleTapThreshold = std::chrono::milliseconds(350);

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string MakeCommandId(const std::string& type) {
    static std::uint64_t counter = 0;
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::ostringstream out;
    out << type << "-" << millis << "-" << ++counter;
    return out.str();
}

std::string SanitizeFilename(std::string value) {
    for (char& ch : value) {
        const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                             (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        if (!allowed) {
            ch = '_';
        }
    }
    return value;
}

std::optional<std::string> ExtractQuotedString(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    const auto first_quote = object_text.find('"', colon + 1);
    if (colon == std::string::npos || first_quote == std::string::npos) {
        return std::nullopt;
    }
    std::string value;
    bool escaping = false;
    for (std::size_t i = first_quote + 1; i < object_text.size(); ++i) {
        const char ch = object_text[i];
        if (escaping) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(ch); break;
            }
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return std::nullopt;
}

std::optional<double> ExtractNumber(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const auto start = object_text.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    const auto end = object_text.find_first_not_of("0123456789.-", start);
    try {
        return std::stod(object_text.substr(start, end - start));
    } catch (...) {
        return std::nullopt;
    }
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

std::optional<Id> ExtractId(const std::string& object_text, const std::string& key) {
    const auto number = ExtractNumber(object_text, key);
    if (!number.has_value() || *number < 0.0) {
        return std::nullopt;
    }
    return static_cast<Id>(*number);
}

std::vector<Id> ExtractIdArray(const std::string& object_text, const std::string& key) {
    std::vector<Id> ids;
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return ids;
    }
    const auto array_start = object_text.find('[', key_pos + needle.size());
    if (array_start == std::string::npos) {
        return ids;
    }
    const auto array_end = object_text.find(']', array_start + 1);
    if (array_end == std::string::npos) {
        return ids;
    }
    std::stringstream stream(object_text.substr(array_start + 1, array_end - array_start - 1));
    std::string token;
    while (std::getline(stream, token, ',')) {
        std::stringstream trimmed(token);
        Id id {};
        if (trimmed >> id) {
            ids.push_back(id);
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
    if (object_start == std::string::npos) {
        return std::nullopt;
    }
    int depth = 0;
    for (std::size_t i = object_start; i < object_text.size(); ++i) {
        if (object_text[i] == '{') {
            ++depth;
        } else if (object_text[i] == '}') {
            --depth;
            if (depth == 0) {
                const auto inner = object_text.substr(object_start, i - object_start + 1);
                dbd::Vec3 value {};
                value.x = static_cast<float>(ExtractNumber(inner, "x").value_or(0.0));
                value.y = static_cast<float>(ExtractNumber(inner, "y").value_or(0.0));
                value.z = static_cast<float>(ExtractNumber(inner, "z").value_or(0.0));
                return value;
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> SplitArrayObjects(const std::string& raw, const std::string& key) {
    std::vector<std::string> objects;
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = raw.find(needle);
    if (key_pos == std::string::npos) {
        return objects;
    }
    const auto array_start = raw.find('[', key_pos + needle.size());
    if (array_start == std::string::npos) {
        return objects;
    }
    int depth = 0;
    std::size_t object_start = std::string::npos;
    for (std::size_t i = array_start + 1; i < raw.size(); ++i) {
        const char ch = raw[i];
        if (ch == '{') {
            if (depth == 0) {
                object_start = i;
            }
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(raw.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    return objects;
}

std::vector<dbd::AutomationRule> ParseAutomationRulesArray(const std::string& object_text, const std::string& key) {
    std::vector<dbd::AutomationRule> rules;
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return rules;
    }
    const auto array_start = object_text.find('[', key_pos + needle.size());
    if (array_start == std::string::npos) {
        return rules;
    }
    int depth = 0;
    std::size_t rule_start = std::string::npos;
    for (std::size_t i = array_start + 1; i < object_text.size(); ++i) {
        const char ch = object_text[i];
        if (ch == '{') {
            if (depth == 0) {
                rule_start = i;
            }
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && rule_start != std::string::npos) {
                const auto rule_text = object_text.substr(rule_start, i - rule_start + 1);
                dbd::AutomationRule rule;
                const auto trigger = ExtractQuotedString(rule_text, "trigger").value_or("LowHealth");
                const auto action = ExtractQuotedString(rule_text, "action").value_or("Retreat");
                rule.threshold = static_cast<float>(ExtractNumber(rule_text, "threshold").value_or(0.0));
                rule.enabled = ExtractBool(rule_text, "enabled").value_or(true);
                if (trigger == "InventoryHeavy") {
                    rule.trigger = dbd::AutomationTrigger::InventoryHeavy;
                } else if (trigger == "InventoryFull") {
                    rule.trigger = dbd::AutomationTrigger::InventoryFull;
                } else if (trigger == "EnemySeen") {
                    rule.trigger = dbd::AutomationTrigger::EnemySeen;
                } else {
                    rule.trigger = dbd::AutomationTrigger::LowHealth;
                }
                if (action == "ReturnToStorage") {
                    rule.action = dbd::AutomationAction::ReturnToStorage;
                } else if (action == "HoldPosition") {
                    rule.action = dbd::AutomationAction::HoldPosition;
                } else if (action == "AttackNearestEnemy") {
                    rule.action = dbd::AutomationAction::AttackNearestEnemy;
                } else {
                    rule.action = dbd::AutomationAction::Retreat;
                }
                rules.push_back(rule);
                rule_start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    return rules;
}

std::vector<PlayerView> ParsePlayersArray(const std::string& raw, const std::string& key) {
    std::vector<PlayerView> players;
    for (const auto& object_text : SplitArrayObjects(raw, key)) {
        PlayerView player;
        player.player_id = ExtractId(object_text, "playerId").value_or(0);
        player.name = ExtractQuotedString(object_text, "name").value_or("Player");
        player.is_primary = ExtractBool(object_text, "isPrimary").value_or(false);
        player.is_raider = ExtractBool(object_text, "isRaider").value_or(false);
        player.credits = static_cast<float>(ExtractNumber(object_text, "credits").value_or(0.0));
        player.tax_load = static_cast<float>(ExtractNumber(object_text, "taxLoad").value_or(0.0));
        player.upkeep_load = static_cast<float>(ExtractNumber(object_text, "upkeepLoad").value_or(0.0));
        player.credit_load = static_cast<float>(ExtractNumber(object_text, "creditLoad").value_or(player.tax_load + player.upkeep_load));
        player.complexity_load = static_cast<float>(ExtractNumber(object_text, "complexityLoad").value_or(0.0));
        player.food_load = static_cast<float>(ExtractNumber(object_text, "foodLoad").value_or(0.0));
        player.food_shortage = static_cast<float>(ExtractNumber(object_text, "foodShortage").value_or(0.0));
        player.upkeep_shortage = static_cast<float>(ExtractNumber(object_text, "upkeepShortage").value_or(0.0));
        player.shortage_ratio = static_cast<float>(ExtractNumber(object_text, "shortageRatio").value_or(std::max(player.food_shortage, player.upkeep_shortage)));
        player.efficiency = static_cast<float>(ExtractNumber(object_text, "efficiency").value_or(1.0));
        player.pressure_ratio = static_cast<float>(ExtractNumber(object_text, "pressureRatio").value_or(0.0));
        players.push_back(player);
    }
    return players;
}

std::optional<std::string> ExtractObjectText(const std::string& raw, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = raw.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto object_start = raw.find('{', key_pos + needle.size());
    if (object_start == std::string::npos) {
        return std::nullopt;
    }
    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t i = object_start; i < raw.size(); ++i) {
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
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return raw.substr(object_start, i - object_start + 1);
            }
        }
    }
    return std::nullopt;
}

WorldSnapshotView ParseSnapshot(const std::string& raw) {
    WorldSnapshotView snapshot;
    snapshot.players = ParsePlayersArray(raw, "players");

    for (const auto& object_text : SplitArrayObjects(raw, "squads")) {
        SquadView squad;
        squad.squad_id = ExtractId(object_text, "squadId").value_or(0);
        squad.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        squad.commander_player_id = ExtractId(object_text, "commanderPlayerId").value_or(0);
        squad.leader_unit_id = ExtractId(object_text, "leaderUnitId").value_or(0);
        squad.name = ExtractQuotedString(object_text, "name").value_or("Squad");
        squad.stance = ExtractQuotedString(object_text, "stance").value_or("Defensive");
        squad.unit_ids = ExtractIdArray(object_text, "unitIds");
        squad.automation_rules = ParseAutomationRulesArray(object_text, "automationRules");
        snapshot.squads.push_back(squad);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "units")) {
        UnitView unit;
        unit.unit_id = ExtractId(object_text, "unitId").value_or(0);
        unit.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        unit.controller_player_id = ExtractId(object_text, "controllerPlayerId").value_or(unit.owner_player_id);
        unit.squad_id = ExtractId(object_text, "squadId").value_or(0);
        unit.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        unit.move_target = ExtractVec3(object_text, "moveTarget").value_or(unit.position);
        unit.order = ExtractQuotedString(object_text, "order").value_or("Idle");
        unit.combat_band = ExtractQuotedString(object_text, "combatBand").value_or("Mid");
        unit.health = static_cast<float>(ExtractNumber(object_text, "health").value_or(0.0));
        unit.max_health = static_cast<float>(ExtractNumber(object_text, "maxHealth").value_or(100.0));
        unit.stamina = static_cast<float>(ExtractNumber(object_text, "stamina").value_or(0.0));
        unit.max_stamina = static_cast<float>(ExtractNumber(object_text, "maxStamina").value_or(100.0));
        unit.cargo_weight = static_cast<float>(ExtractNumber(object_text, "cargoWeight").value_or(0.0));
        unit.cargo_primary_item_id = ExtractId(object_text, "cargoPrimaryItemId").value_or(0);
        unit.cargo_primary_item_name = ExtractQuotedString(object_text, "cargoPrimaryItemName").value_or("Empty");
        unit.carry_capacity = static_cast<float>(ExtractNumber(object_text, "carryCapacity").value_or(40.0));
        unit.vision_range = static_cast<float>(ExtractNumber(object_text, "visionRange").value_or(14.0));
        unit.alive = ExtractBool(object_text, "alive").value_or(true);
        unit.assignment_kind = static_cast<int>(ExtractNumber(object_text, "assignmentKind").value_or(0.0));
        unit.assignment_target_entity_id = ExtractId(object_text, "assignmentTargetEntityId").value_or(0);
        unit.assignment_radius = static_cast<float>(ExtractNumber(object_text, "assignmentRadius").value_or(0.0));
        unit.assignment_patrol_route = ExtractBool(object_text, "assignmentPatrolRoute").value_or(false);
        unit.queued_order_count = static_cast<int>(ExtractNumber(object_text, "queuedOrderCount").value_or(0.0));
        unit.next_queued_order = ExtractQuotedString(object_text, "nextQueuedOrder").value_or("None");
        unit.next_queued_target = ExtractVec3(object_text, "nextQueuedTarget").value_or(unit.move_target);
        unit.tactical_state = ExtractQuotedString(object_text, "tacticalState").value_or("");
        unit.queue_interrupt_reason = ExtractQuotedString(object_text, "queueInterruptReason").value_or("");
        unit.route_active = ExtractBool(object_text, "routeActive").value_or(false);
        unit.route_source_kind = ExtractQuotedString(object_text, "routeSourceKind").value_or("None");
        unit.route_source_id = ExtractId(object_text, "routeSourceId").value_or(0);
        unit.route_storage_id = ExtractId(object_text, "routeStorageId").value_or(0);
        unit.route_phase = ExtractQuotedString(object_text, "routePhase").value_or("None");
        unit.route_threat_level = ExtractQuotedString(object_text, "routeThreatLevel").value_or("safe");
        unit.route_threat_enemy_id = ExtractId(object_text, "routeThreatEnemyId").value_or(0);
        unit.assignment_target_position = ExtractVec3(object_text, "assignmentTargetPosition").value_or(unit.move_target);
        unit.assignment_secondary_position = ExtractVec3(object_text, "assignmentSecondaryPosition").value_or(unit.move_target);
        unit.automation_rules = ParseAutomationRulesArray(object_text, "automationRules");
        snapshot.units.push_back(unit);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "knownContacts")) {
        KnownContactView contact;
        contact.observing_player_id = ExtractId(object_text, "observingPlayerId").value_or(0);
        contact.target_entity_id = ExtractId(object_text, "targetEntityId").value_or(0);
        contact.target_kind = ExtractQuotedString(object_text, "targetKind").value_or("Unit");
        contact.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        contact.last_seen_tick = static_cast<std::uint64_t>(ExtractNumber(object_text, "lastSeenTick").value_or(0.0));
        contact.spotted_by_unit_id = ExtractId(object_text, "spottedByUnitId").value_or(0);
        contact.freshness = ExtractQuotedString(object_text, "freshness").value_or("stale");
        snapshot.known_contacts.push_back(contact);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "flattenJobs")) {
        FlattenJobView job;
        job.flatten_job_id = ExtractId(object_text, "flattenJobId").value_or(0);
        job.center = ExtractVec3(object_text, "center").value_or(dbd::Vec3 {});
        job.radius = static_cast<float>(ExtractNumber(object_text, "radius").value_or(0.0));
        job.required_labor = static_cast<float>(ExtractNumber(object_text, "requiredLabor").value_or(0.0));
        job.accumulated_labor = static_cast<float>(ExtractNumber(object_text, "accumulatedLabor").value_or(0.0));
        job.assigned_count = static_cast<std::size_t>(ExtractNumber(object_text, "assignedCount").value_or(0.0));
        job.state = ExtractQuotedString(object_text, "state").value_or("Planned");
        snapshot.flatten_jobs.push_back(job);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "constructionSites")) {
        ConstructionSiteView site;
        site.construction_site_id = ExtractId(object_text, "constructionSiteId").value_or(0);
        site.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        site.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        site.footprint_radius = static_cast<float>(ExtractNumber(object_text, "footprintRadius").value_or(6.0));
        site.health = static_cast<float>(ExtractNumber(object_text, "health").value_or(0.0));
        site.max_health = static_cast<float>(ExtractNumber(object_text, "maxHealth").value_or(1.0));
        site.completion_ratio = static_cast<float>(ExtractNumber(object_text, "completionRatio").value_or(0.0));
        site.auto_build = ExtractBool(object_text, "autoBuild").value_or(true);
        site.stage = ExtractQuotedString(object_text, "stage").value_or("Planned");
        site.structure_type = ExtractQuotedString(object_text, "structureType").value_or("StorageDepot");
        snapshot.construction_sites.push_back(site);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "structures")) {
        StructureView structure;
        structure.structure_id = ExtractId(object_text, "structureId").value_or(0);
        structure.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        structure.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        structure.health = static_cast<float>(ExtractNumber(object_text, "health").value_or(0.0));
        structure.max_health = static_cast<float>(ExtractNumber(object_text, "maxHealth").value_or(1.0));
        structure.structure_type = ExtractQuotedString(object_text, "structureType").value_or("StorageDepot");
        snapshot.structures.push_back(structure);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "storageSites")) {
        StorageSiteView storage;
        storage.storage_site_id = ExtractId(object_text, "storageSiteId").value_or(0);
        storage.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        storage.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        storage.stored_stack_count = static_cast<std::size_t>(ExtractNumber(object_text, "storedStackCount").value_or(0.0));
        storage.primary_item_id = ExtractId(object_text, "primaryItemId").value_or(0);
        storage.primary_item_name = ExtractQuotedString(object_text, "primaryItemName").value_or("Empty");
        snapshot.storage_sites.push_back(storage);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "droppedCargo")) {
        DroppedCargoView drop;
        drop.dropped_cargo_id = ExtractId(object_text, "droppedCargoId").value_or(0);
        drop.source_unit_id = ExtractId(object_text, "sourceUnitId").value_or(0);
        drop.primary_item_id = ExtractId(object_text, "primaryItemId").value_or(0);
        drop.primary_item_name = ExtractQuotedString(object_text, "primaryItemName").value_or("Unknown cargo");
        drop.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        drop.stack_count = static_cast<std::size_t>(ExtractNumber(object_text, "stackCount").value_or(0.0));
        drop.total_weight = static_cast<float>(ExtractNumber(object_text, "totalWeight").value_or(0.0));
        drop.total_value = static_cast<float>(ExtractNumber(object_text, "totalValue").value_or(0.0));
        snapshot.dropped_cargo.push_back(drop);
    }

    for (const auto& object_text : SplitArrayObjects(raw, "resourceNodes")) {
        ResourceNodeView node;
        node.resource_node_id = ExtractId(object_text, "resourceNodeId").value_or(0);
        node.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        node.risk_band = ExtractQuotedString(object_text, "riskBand").value_or("Low");
        node.produces_item_id = ExtractId(object_text, "producesItemId").value_or(0);
        node.produces_item_name = ExtractQuotedString(object_text, "producesItemName").value_or("Unknown resource");
        node.richness = static_cast<float>(ExtractNumber(object_text, "richness").value_or(1.0));
        node.extraction_rate = static_cast<float>(ExtractNumber(object_text, "extractionRate").value_or(1.0));
        node.remaining_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "remainingAmount").value_or(0.0));
        node.max_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "maxAmount").value_or(0.0));
        snapshot.resource_nodes.push_back(node);
    }

    return snapshot;
}

SessionStatusView ParseSessionStatus(const std::string& raw) {
    SessionStatusView session;
    session.tick = static_cast<std::uint64_t>(ExtractNumber(raw, "tick").value_or(0.0));
    session.mode = ExtractQuotedString(raw, "mode").value_or("unknown");
        session.last_command_ok = ExtractBool(raw, "lastCommandOk").value_or(false);
        session.last_command_message = ExtractQuotedString(raw, "lastCommandMessage").value_or("");
        session.runtime_alert = ExtractQuotedString(raw, "runtimeAlert").value_or("");
        session.supply_alert = ExtractQuotedString(raw, "supplyAlert").value_or("");
        session.combat_summary = ExtractQuotedString(raw, "combatSummary").value_or("");
        session.combat_alert = ExtractQuotedString(raw, "combatAlert").value_or("");
        session.contact_summary = ExtractQuotedString(raw, "contactSummary").value_or("");
        session.contact_alert = ExtractQuotedString(raw, "contactAlert").value_or("");
        session.repair_alert = ExtractQuotedString(raw, "repairAlert").value_or("");
    if (const auto scenario_text = ExtractObjectText(raw, "scenario"); scenario_text.has_value()) {
        session.scenario.name = ExtractQuotedString(*scenario_text, "name").value_or("Depot Run v0");
        session.scenario.state = ExtractQuotedString(*scenario_text, "state").value_or("active");
        session.scenario.reason = ExtractQuotedString(*scenario_text, "reason").value_or("in_progress");
        session.scenario.summary = ExtractQuotedString(*scenario_text, "summary").value_or("");
        session.scenario.iron_fitting_stored = static_cast<float>(ExtractNumber(*scenario_text, "ironFittingStored").value_or(0.0));
        session.scenario.iron_fitting_goal = static_cast<float>(ExtractNumber(*scenario_text, "ironFittingGoal").value_or(15.0));
        session.scenario.iron_node_id = ExtractId(*scenario_text, "ironNodeId").value_or(0);
        session.scenario.iron_node_position = ExtractVec3(*scenario_text, "ironNodePosition").value_or(dbd::Vec3 {});
        session.scenario.forward_depot_complete = ExtractBool(*scenario_text, "forwardDepotComplete").value_or(false);
        session.scenario.living_units = static_cast<std::size_t>(ExtractNumber(*scenario_text, "livingUnits").value_or(0.0));
        session.scenario.units_lost = static_cast<std::size_t>(ExtractNumber(*scenario_text, "unitsLost").value_or(0.0));
        session.scenario.primary_depot_health = static_cast<float>(ExtractNumber(*scenario_text, "primaryDepotHealth").value_or(0.0));
        session.scenario.primary_depot_max_health = static_cast<float>(ExtractNumber(*scenario_text, "primaryDepotMaxHealth").value_or(0.0));
        session.scenario.time_limit_ticks = static_cast<std::uint64_t>(ExtractNumber(*scenario_text, "timeLimitTicks").value_or(1200.0));
        session.scenario.remaining_ticks = static_cast<std::uint64_t>(ExtractNumber(*scenario_text, "remainingTicks").value_or(1200.0));
        session.has_scenario = true;
    }
    const std::string needle = "\"economy\"";
    const auto key_pos = raw.find(needle);
    if (key_pos != std::string::npos) {
        const auto object_start = raw.find('{', key_pos + needle.size());
        if (object_start != std::string::npos) {
            int depth = 0;
            for (std::size_t i = object_start; i < raw.size(); ++i) {
                if (raw[i] == '{') {
                    ++depth;
                } else if (raw[i] == '}') {
                    --depth;
                    if (depth == 0) {
                        const auto object_text = raw.substr(object_start, i - object_start + 1);
                        session.economy.player_id = ExtractId(object_text, "playerId").value_or(0);
                        session.economy.credits = static_cast<float>(ExtractNumber(object_text, "credits").value_or(0.0));
                        session.economy.tax_load = static_cast<float>(ExtractNumber(object_text, "taxLoad").value_or(0.0));
                        session.economy.upkeep_load = static_cast<float>(ExtractNumber(object_text, "upkeepLoad").value_or(0.0));
                        session.economy.credit_load = static_cast<float>(ExtractNumber(object_text, "creditLoad").value_or(0.0));
                        session.economy.complexity_load = static_cast<float>(ExtractNumber(object_text, "complexityLoad").value_or(0.0));
                        session.economy.food_load = static_cast<float>(ExtractNumber(object_text, "foodLoad").value_or(0.0));
                        session.economy.food_shortage = static_cast<float>(ExtractNumber(object_text, "foodShortage").value_or(0.0));
                        session.economy.upkeep_shortage = static_cast<float>(ExtractNumber(object_text, "upkeepShortage").value_or(0.0));
                        session.economy.shortage_ratio = static_cast<float>(ExtractNumber(object_text, "shortageRatio").value_or(0.0));
                        session.economy.efficiency = static_cast<float>(ExtractNumber(object_text, "efficiency").value_or(1.0));
                        session.economy.pressure_ratio = static_cast<float>(ExtractNumber(object_text, "pressureRatio").value_or(0.0));
                        session.economy.pressure_band = ExtractQuotedString(object_text, "pressureBand").value_or("stable");
                        session.has_economy = true;
                        break;
                    }
                }
            }
        }
    }
    return session;
}

CommandFeedbackView ParseCommandReceipt(const std::string& raw, std::filesystem::file_time_type updated_at) {
    CommandFeedbackView feedback;
    feedback.command_id = ExtractQuotedString(raw, "commandId").value_or("");
    feedback.status = ExtractQuotedString(raw, "status").value_or("received");
    feedback.message = ExtractQuotedString(raw, "message").value_or("");
    feedback.updated_at = updated_at;
    feedback.loaded = !feedback.command_id.empty();
    return feedback;
}

CommandFeedbackView ParseCommandResult(const std::string& raw, std::filesystem::file_time_type updated_at) {
    CommandFeedbackView feedback;
    feedback.command_id = ExtractQuotedString(raw, "commandId").value_or("");
    const auto ok = ExtractBool(raw, "ok");
    feedback.status = ok.has_value() ? (*ok ? "ok" : "failed") : "completed";
    feedback.message = ExtractQuotedString(raw, "message").value_or("");
    feedback.updated_at = updated_at;
    feedback.loaded = !feedback.command_id.empty();
    return feedback;
}

std::vector<PlayerView> ParseSessionManifestPlayers(const std::string& raw) {
    return ParsePlayersArray(raw, "players");
}

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const auto count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), count);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

RECT GetBoardRect(HWND hwnd) {
    RECT client {};
    GetClientRect(hwnd, &client);
    RECT board {};
    board.left = 12;
    board.top = kHeaderHeight;
    board.right = std::max<LONG>(board.left + 100, client.right - kPanelWidth - 12);
    board.bottom = client.bottom - 12;
    return board;
}

RECT GetPanelRect(HWND hwnd) {
    RECT client {};
    GetClientRect(hwnd, &client);
    RECT panel {};
    panel.left = std::max<LONG>(120, client.right - kPanelWidth);
    panel.top = kHeaderHeight;
    panel.right = client.right - 12;
    panel.bottom = client.bottom - 12;
    return panel;
}

float WorldToScreenX(const AppState& app, float world_x, const RECT& board) {
    const float center_x = (static_cast<float>(board.left) + static_cast<float>(board.right)) * 0.5f;
    return center_x + (world_x - app.camera.center_x) * app.camera.zoom;
}

float WorldToScreenY(const AppState& app, float world_z, const RECT& board) {
    const float center_y = (static_cast<float>(board.top) + static_cast<float>(board.bottom)) * 0.5f;
    return center_y + (world_z - app.camera.center_z) * app.camera.zoom;
}

dbd::Vec3 ScreenToWorld(const AppState& app, const POINT& point, const RECT& board) {
    const float center_x = (static_cast<float>(board.left) + static_cast<float>(board.right)) * 0.5f;
    const float center_y = (static_cast<float>(board.top) + static_cast<float>(board.bottom)) * 0.5f;
    dbd::Vec3 value {};
    value.x = app.camera.center_x + (static_cast<float>(point.x) - center_x) / app.camera.zoom;
    value.z = app.camera.center_z + (static_cast<float>(point.y) - center_y) / app.camera.zoom;
    return value;
}

COLORREF PlayerColor(Id player_id) {
    constexpr std::array<COLORREF, 6> colors = {
        RGB(96, 165, 250),
        RGB(248, 113, 113),
        RGB(74, 222, 128),
        RGB(250, 204, 21),
        RGB(192, 132, 252),
        RGB(45, 212, 191)
    };
    return colors[static_cast<std::size_t>(player_id % colors.size())];
}

COLORREF HealthColor(float health, float max_health) {
    const float ratio = max_health > 0.0f ? health / max_health : 0.0f;
    if (ratio <= 0.35f) {
        return RGB(248, 113, 113);
    }
    if (ratio <= 0.65f) {
        return RGB(251, 191, 36);
    }
    return RGB(74, 222, 128);
}

void DrawHealthBar(HDC hdc, int x, int y, int width, int height, float health, float max_health) {
    RECT bg {x, y, x + width, y + height};
    HBRUSH bg_brush = CreateSolidBrush(RGB(31, 41, 55));
    FillRect(hdc, &bg, bg_brush);
    DeleteObject(bg_brush);

    const float ratio = std::clamp(max_health > 0.0f ? health / max_health : 0.0f, 0.0f, 1.0f);
    RECT fg {x, y, x + static_cast<int>(std::round(width * ratio)), y + height};
    HBRUSH fg_brush = CreateSolidBrush(HealthColor(health, max_health));
    FillRect(hdc, &fg, fg_brush);
    DeleteObject(fg_brush);
}

bool IsSelected(const AppState& app, Id unit_id) {
    return std::find(app.selected_units.begin(), app.selected_units.end(), unit_id) != app.selected_units.end();
}

const UnitView* FindUnitById(const AppState& app, Id unit_id) {
    const auto it = std::find_if(app.snapshot.units.begin(), app.snapshot.units.end(), [unit_id](const UnitView& unit) {
        return unit.unit_id == unit_id;
    });
    return it == app.snapshot.units.end() ? nullptr : &(*it);
}

bool IsDepotRunObjectiveCargo(const UnitView& unit) {
    return unit.cargo_primary_item_id == kIronFittingItemId ||
        unit.cargo_primary_item_name == "Iron Fitting";
}

bool IsDepotRunObjectiveNode(const AppState& app, const ResourceNodeView& node) {
    return app.session.has_scenario &&
        app.session.scenario.iron_node_id != 0 &&
        node.resource_node_id == app.session.scenario.iron_node_id;
}

bool IsObjectiveCargoThreatened(const AppState& app, const UnitView& objective_carrier) {
    if (objective_carrier.order == "AttackTarget" ||
        objective_carrier.route_threat_level == "threatened" ||
        objective_carrier.route_threat_level == "critical") {
        return true;
    }
    constexpr float kObjectiveCargoThreatRadiusSq = 28.0f * 28.0f;
    for (const auto& other : app.snapshot.units) {
        if (!other.alive || other.owner_player_id == objective_carrier.owner_player_id) {
            continue;
        }
        const float dx = other.position.x - objective_carrier.position.x;
        const float dy = other.position.y - objective_carrier.position.y;
        const float dz = other.position.z - objective_carrier.position.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) <= kObjectiveCargoThreatRadiusSq) {
            return true;
        }
    }
    return false;
}

const SquadView* FindSquadById(const AppState& app, Id squad_id) {
    const auto it = std::find_if(app.snapshot.squads.begin(), app.snapshot.squads.end(), [squad_id](const SquadView& squad) {
        return squad.squad_id == squad_id;
    });
    return it == app.snapshot.squads.end() ? nullptr : &(*it);
}

const PlayerView* FindPlayerById(const AppState& app, Id player_id) {
    const auto it = std::find_if(app.snapshot.players.begin(), app.snapshot.players.end(), [player_id](const PlayerView& player) {
        return player.player_id == player_id;
    });
    return it == app.snapshot.players.end() ? nullptr : &(*it);
}

const ConstructionSiteView* FindConstructionById(const AppState& app, Id site_id) {
    const auto it = std::find_if(app.snapshot.construction_sites.begin(), app.snapshot.construction_sites.end(), [site_id](const ConstructionSiteView& site) {
        return site.construction_site_id == site_id;
    });
    return it == app.snapshot.construction_sites.end() ? nullptr : &(*it);
}

const StructureView* FindStructureById(const AppState& app, Id structure_id) {
    const auto it = std::find_if(app.snapshot.structures.begin(), app.snapshot.structures.end(), [structure_id](const StructureView& structure) {
        return structure.structure_id == structure_id;
    });
    return it == app.snapshot.structures.end() ? nullptr : &(*it);
}

std::wstring PressureBandLabel(const std::string& pressure_band) {
    if (pressure_band == "high") {
        return L"High";
    }
    if (pressure_band == "medium") {
        return L"Medium";
    }
    return L"Stable";
}

float ViewDistanceSquared(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float CombatRangeForBand(const std::string& band) {
    if (band == "Short") {
        return 5.05f;
    }
    if (band == "Long") {
        return 18.55f;
    }
    return 11.05f;
}

std::wstring CombatBandLabel(const std::string& band) {
    if (band == "Short") {
        return L"Short";
    }
    if (band == "Long") {
        return L"Long";
    }
    return L"Mid";
}

std::optional<dbd::Vec3> AttackTargetPosition(const AppState& app, const UnitView& unit) {
    if (unit.assignment_target_entity_id == 0) {
        return std::nullopt;
    }
    if (const UnitView* target_unit = FindUnitById(app, unit.assignment_target_entity_id); target_unit != nullptr) {
        return target_unit->position;
    }
    if (const ConstructionSiteView* site = FindConstructionById(app, unit.assignment_target_entity_id); site != nullptr) {
        return site->position;
    }
    if (const StructureView* structure = FindStructureById(app, unit.assignment_target_entity_id); structure != nullptr) {
        return structure->position;
    }
    return std::nullopt;
}

const char* AutomationTriggerName(dbd::AutomationTrigger trigger) {
    switch (trigger) {
        case dbd::AutomationTrigger::LowHealth: return "LowHealth";
        case dbd::AutomationTrigger::InventoryHeavy: return "InventoryHeavy";
        case dbd::AutomationTrigger::InventoryFull: return "InventoryFull";
        case dbd::AutomationTrigger::EnemySeen: return "EnemySeen";
        default: return "Unknown";
    }
}

const char* AutomationActionName(dbd::AutomationAction action) {
    switch (action) {
        case dbd::AutomationAction::Retreat: return "Retreat";
        case dbd::AutomationAction::ReturnToStorage: return "ReturnToStorage";
        case dbd::AutomationAction::HoldPosition: return "HoldPosition";
        case dbd::AutomationAction::AttackNearestEnemy: return "AttackNearestEnemy";
        default: return "Unknown";
    }
}

std::wstring AutomationScopeName(AutomationScope scope) {
    switch (scope) {
        case AutomationScope::SelectedUnits: return L"Selected Units";
        case AutomationScope::ActiveGroup: return L"Active Group";
        case AutomationScope::Squad: return L"Squad";
        default: return L"Unknown";
    }
}

std::wstring AutomationPresetName(AutomationPreset preset) {
    switch (preset) {
        case AutomationPreset::Gather: return L"Gather";
        case AutomationPreset::Escort: return L"Escort";
        case AutomationPreset::Scout: return L"Scout";
        case AutomationPreset::Hold: return L"Hold";
        default: return L"Unknown";
    }
}

const char* AutomationPresetNameAscii(AutomationPreset preset) {
    switch (preset) {
        case AutomationPreset::Gather: return "Gather";
        case AutomationPreset::Escort: return "Escort";
        case AutomationPreset::Scout: return "Scout";
        case AutomationPreset::Hold: return "Hold";
        default: return "Unknown";
    }
}

std::vector<dbd::AutomationRule> AutomationPresetRules(AutomationPreset preset) {
    switch (preset) {
        case AutomationPreset::Gather:
            return {
                dbd::AutomationRule {dbd::AutomationTrigger::InventoryFull, dbd::AutomationAction::ReturnToStorage, 0.90f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::Retreat, 0.0f, true}};
        case AutomationPreset::Escort:
            return {
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::AttackNearestEnemy, 0.0f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.30f, true}};
        case AutomationPreset::Scout:
            return {
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::Retreat, 0.0f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.55f, true}};
        case AutomationPreset::Hold:
        default:
            return {
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::HoldPosition, 0.0f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.25f, true}};
    }
}

std::vector<Id> AutomationTargetUnitIds(const AppState& app) {
    if (app.automation_editor.scope == AutomationScope::SelectedUnits) {
        return app.selected_units;
    }
    if (app.automation_editor.scope == AutomationScope::ActiveGroup) {
        if (app.active_group_slot >= 0 && app.active_group_slot < static_cast<int>(app.control_groups.size())) {
            return app.control_groups[app.active_group_slot];
        }
        return {};
    }
    return {};
}

bool AutomationRulesEqual(const std::vector<dbd::AutomationRule>& lhs, const std::vector<dbd::AutomationRule>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].trigger != rhs[i].trigger ||
            lhs[i].action != rhs[i].action ||
            std::fabs(lhs[i].threshold - rhs[i].threshold) > 0.0001f ||
            lhs[i].enabled != rhs[i].enabled) {
            return false;
        }
    }
    return true;
}

std::wstring MatchingAutomationPresetName(const std::vector<dbd::AutomationRule>& rules) {
    if (AutomationRulesEqual(rules, AutomationPresetRules(AutomationPreset::Gather))) {
        return L"Gather";
    }
    if (AutomationRulesEqual(rules, AutomationPresetRules(AutomationPreset::Escort))) {
        return L"Escort";
    }
    if (AutomationRulesEqual(rules, AutomationPresetRules(AutomationPreset::Scout))) {
        return L"Scout";
    }
    if (AutomationRulesEqual(rules, AutomationPresetRules(AutomationPreset::Hold))) {
        return L"Hold";
    }
    return L"Custom";
}

void EnsureAutomationEditorSelection(AppState& app) {
    if (app.automation_editor.scope == AutomationScope::Squad) {
        if (app.automation_editor.target_squad_id != 0 && FindSquadById(app, app.automation_editor.target_squad_id) != nullptr) {
            return;
        }
        for (Id unit_id : app.selected_units) {
            const UnitView* unit = FindUnitById(app, unit_id);
            if (unit != nullptr && unit->squad_id != 0 && FindSquadById(app, unit->squad_id) != nullptr) {
                app.automation_editor.target_squad_id = unit->squad_id;
                return;
            }
        }
        if (!app.snapshot.squads.empty()) {
            app.automation_editor.target_squad_id = app.snapshot.squads.front().squad_id;
        }
    }
}

void SyncAutomationEditor(AppState& app, bool force) {
    EnsureAutomationEditorSelection(app);
    if (app.automation_editor.dirty && !force) {
        return;
    }

    std::vector<dbd::AutomationRule> source_rules;
    bool mixed = false;
    std::string source_label = "Selected units";
    if (app.automation_editor.scope == AutomationScope::Squad) {
        if (const SquadView* squad = FindSquadById(app, app.automation_editor.target_squad_id); squad != nullptr) {
            source_rules = squad->automation_rules;
            source_label = "Squad #" + std::to_string(squad->squad_id) + " " + squad->name;
        } else {
            source_label = "Squad (none)";
        }
    } else {
        const auto target_unit_ids = AutomationTargetUnitIds(app);
        if (app.automation_editor.scope == AutomationScope::ActiveGroup) {
            source_label = app.active_group_slot >= 0 ? "Group " + std::to_string(app.active_group_slot + 1) : "Group (none)";
        }
        bool first = true;
        for (Id unit_id : target_unit_ids) {
            const UnitView* unit = FindUnitById(app, unit_id);
            if (unit == nullptr) {
                continue;
            }
            if (first) {
                source_rules = unit->automation_rules;
                first = false;
            } else if (!AutomationRulesEqual(source_rules, unit->automation_rules)) {
                mixed = true;
            }
        }
        if (app.automation_editor.scope == AutomationScope::SelectedUnits && !target_unit_ids.empty()) {
            source_label = std::to_string(target_unit_ids.size()) + " selected unit(s)";
        }
    }

    app.automation_editor.working_rules = source_rules;
    app.automation_editor.mixed_source_rules = mixed;
    app.automation_editor.source_label = source_label;
    if (app.automation_editor.working_rules.empty()) {
        app.automation_editor.selected_rule_index = 0;
    } else {
        app.automation_editor.selected_rule_index = std::min(app.automation_editor.selected_rule_index, app.automation_editor.working_rules.size() - 1);
    }
    app.automation_editor.dirty = false;
}

void CycleAutomationScope(AppState& app) {
    switch (app.automation_editor.scope) {
        case AutomationScope::SelectedUnits: app.automation_editor.scope = AutomationScope::ActiveGroup; break;
        case AutomationScope::ActiveGroup: app.automation_editor.scope = AutomationScope::Squad; break;
        case AutomationScope::Squad: app.automation_editor.scope = AutomationScope::SelectedUnits; break;
    }
    SyncAutomationEditor(app, true);
}

void CycleAutomationSquad(AppState& app, int direction) {
    if (app.snapshot.squads.empty()) {
        app.automation_editor.target_squad_id = 0;
        SyncAutomationEditor(app, true);
        return;
    }
    EnsureAutomationEditorSelection(app);
    std::size_t index = 0;
    for (std::size_t i = 0; i < app.snapshot.squads.size(); ++i) {
        if (app.snapshot.squads[i].squad_id == app.automation_editor.target_squad_id) {
            index = i;
            break;
        }
    }
    index = (index + app.snapshot.squads.size() + static_cast<std::size_t>(direction)) % app.snapshot.squads.size();
    app.automation_editor.target_squad_id = app.snapshot.squads[index].squad_id;
    SyncAutomationEditor(app, true);
}

dbd::AutomationTrigger NextAutomationTrigger(dbd::AutomationTrigger trigger) {
    switch (trigger) {
        case dbd::AutomationTrigger::LowHealth: return dbd::AutomationTrigger::InventoryHeavy;
        case dbd::AutomationTrigger::InventoryHeavy: return dbd::AutomationTrigger::InventoryFull;
        case dbd::AutomationTrigger::InventoryFull: return dbd::AutomationTrigger::EnemySeen;
        case dbd::AutomationTrigger::EnemySeen:
        default:
            return dbd::AutomationTrigger::LowHealth;
    }
}

dbd::AutomationAction NextAutomationAction(dbd::AutomationAction action) {
    switch (action) {
        case dbd::AutomationAction::Retreat: return dbd::AutomationAction::ReturnToStorage;
        case dbd::AutomationAction::ReturnToStorage: return dbd::AutomationAction::HoldPosition;
        case dbd::AutomationAction::HoldPosition: return dbd::AutomationAction::AttackNearestEnemy;
        case dbd::AutomationAction::AttackNearestEnemy:
        default:
            return dbd::AutomationAction::Retreat;
    }
}

dbd::AutomationRule* SelectedAutomationRule(AppState& app) {
    if (app.automation_editor.working_rules.empty()) {
        return nullptr;
    }
    return &app.automation_editor.working_rules[app.automation_editor.selected_rule_index];
}

void MarkAutomationDirty(AppState& app) {
    app.automation_editor.dirty = true;
}

bool CanSelectUnit(const AppState& app, const UnitView& unit) {
    if (!unit.alive || app.primary_player_id == 0) {
        return false;
    }
    return unit.owner_player_id == app.primary_player_id || unit.controller_player_id == app.primary_player_id;
}

bool IsFriendlyUnit(const AppState& app, const UnitView& unit) {
    if (app.primary_player_id == 0) {
        return false;
    }
    return unit.owner_player_id == app.primary_player_id || unit.controller_player_id == app.primary_player_id;
}

void PruneSelection(AppState& app);
void SyncAutomationEditor(AppState& app, bool force);

RuntimeSummary BuildRuntimeSummary(const AppState& app) {
    RuntimeSummary summary {};
    for (const auto& unit : app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        if (unit.order == "Harvest" || unit.order == "FlattenSite") {
            ++summary.working_units;
        } else if (unit.order == "AttackTarget") {
            ++summary.combat_units;
        } else if (unit.order == "HaulToStorage") {
            ++summary.hauling_units;
        } else if (unit.order == "Retreat") {
            ++summary.retreating_units;
        }
    }
    return summary;
}

void PruneGroup(std::vector<Id>& group, const AppState& app) {
    group.erase(
        std::remove_if(group.begin(), group.end(), [&app](Id unit_id) {
            const UnitView* unit = FindUnitById(app, unit_id);
            return unit == nullptr || !CanSelectUnit(app, *unit);
        }),
        group.end());
}

void PruneGroups(AppState& app) {
    for (auto& group : app.control_groups) {
        PruneGroup(group, app);
    }
}

void StoreSelectionToGroup(AppState& app, int slot) {
    if (slot < 0 || slot >= static_cast<int>(app.control_groups.size())) {
        return;
    }
    PruneSelection(app);
    app.control_groups[slot] = app.selected_units;
    PruneGroup(app.control_groups[slot], app);
    app.active_group_slot = slot;
    SyncAutomationEditor(app, false);
}

void RecallGroup(AppState& app, int slot, bool additive) {
    if (slot < 0 || slot >= static_cast<int>(app.control_groups.size())) {
        return;
    }
    PruneGroups(app);
    const auto& group = app.control_groups[slot];
    if (!additive) {
        app.selected_units = group;
    } else {
        for (Id unit_id : group) {
            if (!IsSelected(app, unit_id)) {
                app.selected_units.push_back(unit_id);
            }
        }
    }
    app.active_group_slot = slot;
    SyncAutomationEditor(app, false);
}

std::optional<dbd::Vec3> FindGroupCenter(const AppState& app, int slot) {
    if (slot < 0 || slot >= static_cast<int>(app.control_groups.size())) {
        return std::nullopt;
    }

    dbd::Vec3 center {};
    std::size_t count = 0;
    for (Id unit_id : app.control_groups[slot]) {
        const UnitView* unit = FindUnitById(app, unit_id);
        if (unit == nullptr || !CanSelectUnit(app, *unit)) {
            continue;
        }
        center.x += unit->position.x;
        center.y += unit->position.y;
        center.z += unit->position.z;
        ++count;
    }

    if (count == 0) {
        return std::nullopt;
    }

    const float inverse_count = 1.0f / static_cast<float>(count);
    center.x *= inverse_count;
    center.y *= inverse_count;
    center.z *= inverse_count;
    return center;
}

void RecallGroupHotkey(AppState& app, int slot, bool additive, bool is_repeat) {
    RecallGroup(app, slot, additive);

    if (is_repeat) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool is_double_tap = app.last_recalled_group_slot == slot &&
                               (now - app.last_group_recall_time) <= kControlGroupDoubleTapThreshold;
    app.last_recalled_group_slot = slot;
    app.last_group_recall_time = now;

    if (!is_double_tap) {
        return;
    }

    if (const auto center = FindGroupCenter(app, slot); center.has_value()) {
        app.camera.center_x = center->x;
        app.camera.center_z = center->z;
    }
}

const wchar_t* StructureHotkeyName(dbd::StructureType type) {
    switch (type) {
        case dbd::StructureType::StorageDepot: return L"Storage";
        case dbd::StructureType::Extractor: return L"Extractor";
        case dbd::StructureType::DefenseNode: return L"Defense";
        default: return L"Unknown";
    }
}

dbd::StructureType NextStructureType(dbd::StructureType type) {
    switch (type) {
        case dbd::StructureType::StorageDepot: return dbd::StructureType::Extractor;
        case dbd::StructureType::Extractor: return dbd::StructureType::DefenseNode;
        case dbd::StructureType::DefenseNode:
        default:
            return dbd::StructureType::StorageDepot;
    }
}

void PruneSelection(AppState& app) {
    app.selected_units.erase(
        std::remove_if(app.selected_units.begin(), app.selected_units.end(), [&app](Id unit_id) {
            const UnitView* unit = FindUnitById(app, unit_id);
            return unit == nullptr || !CanSelectUnit(app, *unit);
        }),
        app.selected_units.end());
}

std::optional<CommandFeedbackView> ReadLatestCommandFeedbackFromDir(
    const std::filesystem::path& dir,
    bool is_result_dir) {
    std::error_code error;
    if (!std::filesystem::is_directory(dir, error)) {
        return std::nullopt;
    }

    std::optional<CommandFeedbackView> latest;
    for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error) || entry.path().extension() != ".json") {
            continue;
        }
        const auto updated_at = entry.last_write_time(error);
        if (error) {
            error.clear();
            continue;
        }
        if (latest.has_value() && updated_at <= latest->updated_at) {
            continue;
        }
        const auto raw = ReadTextFile(entry.path());
        if (raw.empty()) {
            continue;
        }
        auto parsed = is_result_dir ? ParseCommandResult(raw, updated_at) : ParseCommandReceipt(raw, updated_at);
        if (parsed.loaded) {
            latest = parsed;
        }
    }
    return latest;
}

CommandFeedbackView ReadLatestCommandFeedback(const std::filesystem::path& runtime_dir) {
    const auto spool_dir = runtime_dir / "command_spool";
    const auto receipt = ReadLatestCommandFeedbackFromDir(spool_dir / "receipts", false);
    const auto result = ReadLatestCommandFeedbackFromDir(spool_dir / "results", true);
    if (receipt.has_value() && result.has_value()) {
        return result->updated_at >= receipt->updated_at ? *result : *receipt;
    }
    if (result.has_value()) {
        return *result;
    }
    if (receipt.has_value()) {
        return *receipt;
    }
    return {};
}

std::string StructureTypeName(dbd::StructureType type) {
    switch (type) {
        case dbd::StructureType::Extractor: return "Extractor";
        case dbd::StructureType::DefenseNode: return "DefenseNode";
        case dbd::StructureType::StorageDepot:
        default: return "StorageDepot";
    }
}

std::vector<const KnownContactView*> PrimaryContacts(const AppState& app) {
    std::vector<const KnownContactView*> contacts;
    for (const auto& contact : app.snapshot.known_contacts) {
        if (contact.observing_player_id == app.primary_player_id) {
            contacts.push_back(&contact);
        }
    }
    std::sort(contacts.begin(), contacts.end(), [](const KnownContactView* a, const KnownContactView* b) {
        if (a->freshness != b->freshness) {
            return a->freshness == "visible";
        }
        return a->last_seen_tick > b->last_seen_tick;
    });
    return contacts;
}

const KnownContactView* SelectedContact(const AppState& app) {
    const auto contacts = PrimaryContacts(app);
    if (app.selected_contact_index < 0 || app.selected_contact_index >= static_cast<int>(contacts.size())) {
        return nullptr;
    }
    return contacts[static_cast<std::size_t>(app.selected_contact_index)];
}

void ClampSelectedContact(AppState& app) {
    const auto contacts = PrimaryContacts(app);
    if (contacts.empty()) {
        app.selected_contact_index = -1;
        return;
    }
    if (app.selected_contact_index < 0) {
        app.selected_contact_index = 0;
    } else if (app.selected_contact_index >= static_cast<int>(contacts.size())) {
        app.selected_contact_index = static_cast<int>(contacts.size()) - 1;
    }
}

void CycleSelectedContact(AppState& app, int direction) {
    const auto contacts = PrimaryContacts(app);
    if (contacts.empty()) {
        app.selected_contact_index = -1;
        return;
    }
    if (app.selected_contact_index < 0 || app.selected_contact_index >= static_cast<int>(contacts.size())) {
        app.selected_contact_index = direction >= 0 ? 0 : static_cast<int>(contacts.size()) - 1;
        return;
    }
    const int size = static_cast<int>(contacts.size());
    app.selected_contact_index = (app.selected_contact_index + direction + size) % size;
}

void RefreshSnapshot(AppState& app) {
    const auto snapshot_text = ReadTextFile(app.runtime_dir / "world_snapshot.json");
    const auto session_text = ReadTextFile(app.runtime_dir / "session_status.json");
    const auto manifest_text = ReadTextFile(app.runtime_dir / "session_manifest.json");
    if (!snapshot_text.empty()) {
        app.snapshot = ParseSnapshot(snapshot_text);
        app.snapshot_loaded = true;
        if (!manifest_text.empty()) {
            const auto manifest_players = ParseSessionManifestPlayers(manifest_text);
            if (!manifest_players.empty()) {
                for (auto& player : app.snapshot.players) {
                    const auto manifest_it = std::find_if(
                        manifest_players.begin(),
                        manifest_players.end(),
                        [&player](const PlayerView& manifest_player) { return manifest_player.player_id == player.player_id; });
                    if (manifest_it != manifest_players.end()) {
                        player.is_primary = manifest_it->is_primary;
                        player.is_raider = manifest_it->is_raider;
                        if (!manifest_it->name.empty()) {
                            player.name = manifest_it->name;
                        }
                    }
                }
            }
        }
        if (app.primary_player_id == 0 && !app.snapshot.players.empty()) {
            for (const auto& player : app.snapshot.players) {
                if (player.is_primary) {
                    app.primary_player_id = player.player_id;
                    break;
                }
            }
            if (app.primary_player_id == 0) {
                app.primary_player_id = app.snapshot.players.front().player_id;
            }
        }
        PruneSelection(app);
        ClampSelectedContact(app);
        SyncAutomationEditor(app, false);
    }
    if (!session_text.empty()) {
        app.session = ParseSessionStatus(session_text);
    }
    app.command_feedback = ReadLatestCommandFeedback(app.runtime_dir);
}

void WriteCommandFile(const AppState& app, const std::string& command_id, const std::string& payload) {
    std::filesystem::create_directories(app.runtime_dir);
    const auto inbox_dir = app.runtime_dir / "command_spool" / "inbox";
    std::filesystem::create_directories(inbox_dir);
    const auto command_path = inbox_dir / (SanitizeFilename(command_id) + ".json");
    std::ofstream out(command_path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << payload;
}

std::string JoinIds(const std::vector<Id>& ids) {
    std::ostringstream out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << ids[i];
    }
    return out.str();
}

std::string AutomationRulesJson(const std::vector<dbd::AutomationRule>& rules) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << "{\"trigger\": \"" << AutomationTriggerName(rules[i].trigger)
            << "\", \"action\": \"" << AutomationActionName(rules[i].action)
            << "\", \"threshold\": " << rules[i].threshold
            << ", \"enabled\": " << (rules[i].enabled ? "true" : "false") << "}";
    }
    out << "]";
    return out.str();
}

void IssueAutomationCommand(const AppState& app) {
    if (app.primary_player_id == 0) {
        return;
    }

    const auto rules_json = AutomationRulesJson(app.automation_editor.working_rules);
    if (app.automation_editor.scope == AutomationScope::Squad) {
        if (app.automation_editor.target_squad_id == 0) {
            return;
        }
        const auto command_id = MakeCommandId("set_squad_automation");
        std::ostringstream payload;
        payload << "{\"commandId\": \"" << command_id
                << "\", \"type\": \"set_squad_automation\""
                << ", \"controllerPlayerId\": " << app.primary_player_id
                << ", \"playerId\": " << app.primary_player_id
                << ", \"squadId\": " << app.automation_editor.target_squad_id
                << ", \"automationRules\": " << rules_json
                << "}";
        WriteCommandFile(app, command_id, payload.str());
        return;
    }

    const auto target_unit_ids = AutomationTargetUnitIds(app);
    if (target_unit_ids.empty()) {
        return;
    }
    const auto command_id = MakeCommandId("set_unit_automation");
    std::ostringstream payload;
    payload << "{\"commands\": [";
    for (std::size_t i = 0; i < target_unit_ids.size(); ++i) {
        if (i != 0) {
            payload << ", ";
        }
        payload << "{\"commandId\": \"" << command_id << "-" << (i + 1)
                << "\", \"type\": \"set_unit_automation\""
                << ", \"controllerPlayerId\": " << app.primary_player_id
                << ", \"unitId\": " << target_unit_ids[i]
                << ", \"automationRules\": " << rules_json
                << "}";
    }
    payload << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void ApplyAutomationPreset(AppState& app, AutomationPreset preset) {
    app.automation_editor.working_rules = AutomationPresetRules(preset);
    app.automation_editor.selected_rule_index = 0;
    app.automation_editor.dirty = true;
    app.automation_editor.source_label += " / ";
    app.automation_editor.source_label += AutomationPresetNameAscii(preset);
    IssueAutomationCommand(app);
}

void IssueMoveCommand(const AppState& app, const dbd::Vec3& target) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("move");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"move\", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"target\": {\"x\": " << target.x << ", \"y\": 0, \"z\": " << target.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueFormationMoveCommand(const AppState& app, const dbd::Vec3& target) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("formation_move");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"formation_move\", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"target\": {\"x\": " << target.x << ", \"y\": 0, \"z\": " << target.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueQueueMoveCommand(const AppState& app, const dbd::Vec3& target) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("queue_move");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"queue_move\", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"target\": {\"x\": " << target.x << ", \"y\": 0, \"z\": " << target.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueAttackCommand(const AppState& app, dbd::AttackTargetKind kind, Id target_entity_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || target_entity_id == 0) {
        return;
    }
    std::string kind_name = "Unit";
    if (kind == dbd::AttackTargetKind::ConstructionSite) {
        kind_name = "ConstructionSite";
    } else if (kind == dbd::AttackTargetKind::Structure) {
        kind_name = "Structure";
    }
    const auto command_id = MakeCommandId("focus_fire");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"focus_fire\", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetKind\": \"" << kind_name << "\""
            << ", \"targetEntityId\": " << target_entity_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueInterceptCommand(const AppState& app, Id target_unit_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || target_unit_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("intercept_unit");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"intercept_unit\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetEntityId\": " << target_unit_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueGuardUnitCommand(const AppState& app, Id target_unit_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || target_unit_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("guard_unit");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"guard_unit\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetEntityId\": " << target_unit_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueGuardSiteCommand(const AppState& app, dbd::AttackTargetKind kind, Id target_entity_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || target_entity_id == 0) {
        return;
    }
    std::string kind_name = "Structure";
    if (kind == dbd::AttackTargetKind::ConstructionSite) {
        kind_name = "ConstructionSite";
    }
    const auto command_id = MakeCommandId("guard_site");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"guard_site\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetKind\": \"" << kind_name << "\""
            << ", \"targetEntityId\": " << target_entity_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueHoldPositionCommand(const AppState& app, const dbd::Vec3& position) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("hold_position");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"hold_position\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"target\": {\"x\": " << position.x << ", \"y\": 0, \"z\": " << position.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueStopCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("stop");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"stop\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueClearQueueCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("clear_queue");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"clear_queue\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueRetreatCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("retreat_selected");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"retreat_selected\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueEmergencyDepositCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("emergency_deposit");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"emergency_deposit\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueDropCargoCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("drop_cargo");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"drop_cargo\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueGuardThreatenedRouteCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("guard_threatened_route");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"guard_threatened_route\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueInterceptRouteThreatCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("intercept_route_threat");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"intercept_route_threat\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueScoutAreaCommand(const AppState& app, const dbd::Vec3& center) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("scout_area");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"scout_area\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"center\": {\"x\": " << center.x << ", \"y\": 0, \"z\": " << center.z << "}"
            << ", \"radius\": " << app.pending_scout_radius << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueQueueScoutAreaCommand(const AppState& app, const dbd::Vec3& center) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("queue_scout_area");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"queue_scout_area\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"center\": {\"x\": " << center.x << ", \"y\": 0, \"z\": " << center.z << "}"
            << ", \"radius\": " << app.pending_scout_radius << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssuePatrolRouteCommand(const AppState& app, const dbd::Vec3& point_a, const dbd::Vec3& point_b) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("patrol_route");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"patrol_route\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"pointA\": {\"x\": " << point_a.x << ", \"y\": 0, \"z\": " << point_a.z << "}"
            << ", \"pointB\": {\"x\": " << point_b.x << ", \"y\": 0, \"z\": " << point_b.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueQueuePatrolRouteCommand(const AppState& app, const dbd::Vec3& point_a, const dbd::Vec3& point_b) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("queue_patrol_route");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"queue_patrol_route\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"pointA\": {\"x\": " << point_a.x << ", \"y\": 0, \"z\": " << point_a.z << "}"
            << ", \"pointB\": {\"x\": " << point_b.x << ", \"y\": 0, \"z\": " << point_b.z << "}}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueInvestigateContactCommand(const AppState& app) {
    const KnownContactView* contact = SelectedContact(app);
    if (app.selected_units.empty() || app.primary_player_id == 0 || contact == nullptr) {
        return;
    }
    const auto command_id = MakeCommandId("investigate_contact");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"investigate_contact\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetKind\": \"" << contact->target_kind << "\""
            << ", \"targetEntityId\": " << contact->target_entity_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueHarvestCommand(const AppState& app, const std::vector<Id>& unit_ids, Id resource_node_id) {
    if (app.primary_player_id == 0 || unit_ids.empty() || resource_node_id == 0) {
        return;
    }

    std::vector<Id> valid_unit_ids;
    valid_unit_ids.reserve(unit_ids.size());
    for (const auto unit_id : unit_ids) {
        if (unit_id != 0) {
            valid_unit_ids.push_back(unit_id);
        }
    }
    if (valid_unit_ids.empty()) {
        return;
    }

    const auto command_id = MakeCommandId("harvest");
    std::ostringstream payload;
    payload << "{\"commands\": [";
    for (std::size_t i = 0; i < valid_unit_ids.size(); ++i) {
        if (i != 0) {
            payload << ", ";
        }
        payload << "{\"commandId\": \"" << command_id << "-" << (i + 1) << "\""
                << ", \"type\": \"harvest\""
                << ", \"controllerPlayerId\": " << app.primary_player_id
                << ", \"unitId\": " << valid_unit_ids[i]
                << ", \"resourceNodeId\": " << resource_node_id << "}";
    }
    payload << "]}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueLootCommand(const AppState& app, Id dropped_cargo_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || dropped_cargo_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("loot");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"loot\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"droppedCargoId\": " << dropped_cargo_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueHaulRouteCommand(const AppState& app, const std::string& source_kind, Id source_id, Id storage_site_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || source_id == 0 || source_kind == "None") {
        return;
    }
    const auto command_id = MakeCommandId("haul_route");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"haul_route\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"sourceKind\": \"" << source_kind << "\""
            << ", \"sourceId\": " << source_id;
    if (storage_site_id != 0) {
        payload << ", \"storageSiteId\": " << storage_site_id;
    }
    payload << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueReturnCommand(const AppState& app) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    for (const auto unit_id : app.selected_units) {
        const auto command_id = MakeCommandId("return_to_storage");
        std::ostringstream payload;
        payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"return_to_storage\""
                << ", \"controllerPlayerId\": " << app.primary_player_id
                << ", \"playerId\": " << app.primary_player_id
                << ", \"unitId\": " << unit_id << "}";
        WriteCommandFile(app, command_id, payload.str());
    }
}

void IssueFlattenCommand(const AppState& app, const dbd::Vec3& center) {
    if (app.selected_units.empty() || app.primary_player_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("start_flatten");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"start_flatten\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"playerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"center\": {\"x\": " << center.x << ", \"y\": 0, \"z\": " << center.z << "}"
            << ", \"radius\": " << app.pending_flatten_radius
            << ", \"targetGrade\": 0.12}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueConstructionCommand(const AppState& app, const dbd::Vec3& position) {
    if (app.primary_player_id == 0) {
        return;
    }
    float footprint = 8.0f;
    if (app.pending_structure_type == dbd::StructureType::DefenseNode) {
        footprint = 5.0f;
    } else if (app.pending_structure_type == dbd::StructureType::Extractor) {
        footprint = 7.0f;
    } else {
        footprint = 10.0f;
    }
    const auto command_id = MakeCommandId("start_construction");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"start_construction\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"playerId\": " << app.primary_player_id
            << ", \"structureType\": \"" << StructureTypeName(app.pending_structure_type) << "\""
            << ", \"position\": {\"x\": " << position.x << ", \"y\": 0, \"z\": " << position.z << "}"
            << ", \"footprintRadius\": " << footprint << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueRepairStructureCommand(const AppState& app, Id structure_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || structure_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("repair_structure");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"repair_structure\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetEntityId\": " << structure_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueRepairConstructionSiteCommand(const AppState& app, Id construction_site_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || construction_site_id == 0) {
        return;
    }
    const auto command_id = MakeCommandId("repair_construction_site");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"repair_construction_site\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetEntityId\": " << construction_site_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

void IssueSupplyRepairCommand(const AppState& app, dbd::AttackTargetKind kind, Id target_entity_id) {
    if (app.selected_units.empty() || app.primary_player_id == 0 || target_entity_id == 0) {
        return;
    }
    std::string kind_name = "Structure";
    if (kind == dbd::AttackTargetKind::ConstructionSite) {
        kind_name = "ConstructionSite";
    }
    const auto command_id = MakeCommandId("supply_repair");
    std::ostringstream payload;
    payload << "{\"commandId\": \"" << command_id << "\", \"type\": \"supply_repair\""
            << ", \"controllerPlayerId\": " << app.primary_player_id
            << ", \"unitIds\": [" << JoinIds(app.selected_units) << "]"
            << ", \"targetKind\": \"" << kind_name << "\""
            << ", \"targetEntityId\": " << target_entity_id << "}";
    WriteCommandFile(app, command_id, payload.str());
}

std::optional<UnitView> FindUnitAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& unit : app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const auto sx = WorldToScreenX(app, unit.position.x, board);
        const auto sy = WorldToScreenY(app, unit.position.z, board);
        const float radius = IsSelected(app, unit.unit_id) ? 10.0f : 7.0f;
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= radius * radius) {
            return unit;
        }
    }
    return std::nullopt;
}

std::optional<ResourceNodeView> FindNodeAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& node : app.snapshot.resource_nodes) {
        const auto sx = WorldToScreenX(app, node.position.x, board);
        const auto sy = WorldToScreenY(app, node.position.z, board);
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= 64.0f) {
            return node;
        }
    }
    return std::nullopt;
}

std::optional<DroppedCargoView> FindDroppedCargoAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& drop : app.snapshot.dropped_cargo) {
        const auto sx = WorldToScreenX(app, drop.position.x, board);
        const auto sy = WorldToScreenY(app, drop.position.z, board);
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= 100.0f) {
            return drop;
        }
    }
    return std::nullopt;
}

std::optional<StorageSiteView> FindStorageAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& storage : app.snapshot.storage_sites) {
        const auto sx = WorldToScreenX(app, storage.position.x, board);
        const auto sy = WorldToScreenY(app, storage.position.z, board);
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= 100.0f) {
            return storage;
        }
    }
    return std::nullopt;
}

std::optional<ConstructionSiteView> FindConstructionAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& site : app.snapshot.construction_sites) {
        const auto sx = WorldToScreenX(app, site.position.x, board);
        const auto sy = WorldToScreenY(app, site.position.z, board);
        const float radius = std::max(8.0f, site.footprint_radius * app.camera.zoom * 0.35f);
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= radius * radius) {
            return site;
        }
    }
    return std::nullopt;
}

std::optional<StructureView> FindStructureAtPoint(const AppState& app, const RECT& board, const POINT& point) {
    for (const auto& structure : app.snapshot.structures) {
        const auto sx = WorldToScreenX(app, structure.position.x, board);
        const auto sy = WorldToScreenY(app, structure.position.z, board);
        const float dx = static_cast<float>(point.x) - sx;
        const float dy = static_cast<float>(point.y) - sy;
        if ((dx * dx) + (dy * dy) <= 100.0f) {
            return structure;
        }
    }
    return std::nullopt;
}

void SelectUnitsInRect(AppState& app, const RECT& board, bool additive) {
    if (!additive) {
        app.selected_units.clear();
    }
    RECT drag_rect {};
    drag_rect.left = std::min(app.drag_start_screen.x, app.drag_end_screen.x);
    drag_rect.right = std::max(app.drag_start_screen.x, app.drag_end_screen.x);
    drag_rect.top = std::min(app.drag_start_screen.y, app.drag_end_screen.y);
    drag_rect.bottom = std::max(app.drag_start_screen.y, app.drag_end_screen.y);

    for (const auto& unit : app.snapshot.units) {
        if (!CanSelectUnit(app, unit)) {
            continue;
        }
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, unit.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, unit.position.z, board)));
        if (sx >= drag_rect.left && sx <= drag_rect.right && sy >= drag_rect.top && sy <= drag_rect.bottom) {
            if (!IsSelected(app, unit.unit_id)) {
                app.selected_units.push_back(unit.unit_id);
            }
        }
    }
}

void DrawLabel(HDC hdc, int x, int y, const std::wstring& text, COLORREF color = RGB(229, 231, 235)) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
}

void PaintApp(HWND hwnd, HDC hdc) {
    AppState& app = *g_app;
    PruneSelection(app);
    PruneGroups(app);
    const auto runtime_summary = BuildRuntimeSummary(app);
    RECT client {};
    GetClientRect(hwnd, &client);
    HBRUSH bg = CreateSolidBrush(RGB(15, 23, 42));
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    RECT header {0, 0, client.right, kHeaderHeight - 8};
    HBRUSH header_brush = CreateSolidBrush(RGB(17, 24, 39));
    FillRect(hdc, &header, header_brush);
    DeleteObject(header_brush);

    DrawLabel(hdc, 16, 14, L"DBD Reboot Native Client", RGB(240, 249, 255));
    std::wstring status = L"Mode: ";
    status += ToWide(app.session.mode.empty() ? "play_session" : app.session.mode);
    status += L"  |  Tick: " + std::to_wstring(app.session.tick);
    if (app.command_feedback.loaded) {
        status += L"  |  Last: " + ToWide(app.command_feedback.command_id) + L" " + ToWide(app.command_feedback.status);
    }
    DrawLabel(hdc, 16, 38, status, RGB(148, 163, 184));

    std::wstring command = L"Surface: Free Play  |  Command: ";
    switch (app.command_mode) {
        case CommandMode::Move: command += L"Move"; break;
        case CommandMode::Attack: command += L"Focus Fire"; break;
        case CommandMode::Intercept: command += L"Intercept (click enemy unit)"; break;
        case CommandMode::Harvest: command += L"Harvest"; break;
        case CommandMode::Flatten: command += L"Flatten"; break;
        case CommandMode::Build: command += L"Build"; break;
        case CommandMode::Loot: command += L"Loot (click dropped cargo)"; break;
        case CommandMode::HaulRoute:
            command += app.pending_haul_source_set ? L"Haul Route (click depot or ground for nearest)" : L"Haul Route (click node/drop)";
            break;
        case CommandMode::Guard: command += L"Guard (unit/site/ground)"; break;
        case CommandMode::Repair: command += L"Repair (click damaged structure/site)"; break;
        case CommandMode::RepairSupply: command += L"Supply Repair (click damaged structure/site)"; break;
        case CommandMode::ScoutArea: command += app.queue_next_order ? L"Queue Scout Area (click ground)" : L"Scout Area (click ground)"; break;
        case CommandMode::PatrolRoute: command += app.queue_next_order
            ? (app.pending_patrol_start_set ? L"Queue Patrol Route (click point B)" : L"Queue Patrol Route (click point A)")
            : (app.pending_patrol_start_set ? L"Patrol Route (click point B)" : L"Patrol Route (click point A)"); break;
        case CommandMode::InvestigateContact: command += L"Investigate Contact (J)"; break;
    }
    command += L"  |  Selected: " + std::to_wstring(app.selected_units.size());
    command += L"  |  Structure: " + std::wstring(StructureHotkeyName(app.pending_structure_type));
    command += L"  |  Y Deposit  U Drop  K GuardRoute  Shift+K Intercept  E Retreat";
    command += L"  |  Z Repair  Shift+Z SupplyRepair";
    command += L"  |  Shift+Right queue  Shift+X clear queue";
    command += L"  |  H Haul  C Harvest  O Scout  P Patrol  N Contact  J Investigate";
    if (app.active_group_slot >= 0) {
        command += L"  |  Group: " + std::to_wstring(app.active_group_slot + 1);
    }
    DrawLabel(hdc, 16, 58, command, RGB(196, 181, 253));

    RECT board = GetBoardRect(hwnd);
    HBRUSH board_brush = CreateSolidBrush(RGB(8, 17, 31));
    FillRect(hdc, &board, board_brush);
    DeleteObject(board_brush);
    FrameRect(hdc, &board, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(24, 39, 59));
    SelectObject(hdc, grid_pen);
    for (int gx = board.left; gx < board.right; gx += 32) {
        MoveToEx(hdc, gx, board.top, nullptr);
        LineTo(hdc, gx, board.bottom);
    }
    for (int gy = board.top; gy < board.bottom; gy += 32) {
        MoveToEx(hdc, board.left, gy, nullptr);
        LineTo(hdc, board.right, gy);
    }
    DeleteObject(grid_pen);

    if (app.session.has_scenario &&
        (app.session.scenario.state == "success" || app.session.scenario.state == "failed")) {
        const bool scenario_success = app.session.scenario.state == "success";
        RECT banner {
            board.left + 24,
            board.top + 22,
            std::min(board.right - 24, board.left + 430),
            board.top + 86
        };
        HBRUSH banner_brush = CreateSolidBrush(scenario_success ? RGB(22, 101, 52) : RGB(127, 29, 29));
        FillRect(hdc, &banner, banner_brush);
        DeleteObject(banner_brush);
        FrameRect(hdc, &banner, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        DrawLabel(
            hdc,
            banner.left + 14,
            banner.top + 10,
            scenario_success ? L"DEPOT RUN COMPLETE" : L"DEPOT RUN FAILED",
            RGB(240, 249, 255));
        std::wostringstream banner_line;
        banner_line << L"Iron Fitting secured: "
                    << static_cast<int>(std::round(app.session.scenario.iron_fitting_stored))
                    << L"/" << static_cast<int>(std::round(app.session.scenario.iron_fitting_goal))
                    << L"  |  Reason: " << ToWide(app.session.scenario.reason);
        DrawLabel(hdc, banner.left + 14, banner.top + 34, banner_line.str(), RGB(226, 232, 240));
        std::wostringstream result_line;
        result_line << L"Units lost: " << app.session.scenario.units_lost
                    << L"  |  Depot HP: "
                    << static_cast<int>(std::round(app.session.scenario.primary_depot_health))
                    << L"/"
                    << static_cast<int>(std::round(app.session.scenario.primary_depot_max_health))
                    << L"  |  Time left: " << app.session.scenario.remaining_ticks
                    << L"  |  Forward depot: "
                    << (app.session.scenario.forward_depot_complete ? L"done" : L"incomplete");
        DrawLabel(hdc, banner.left + 14, banner.top + 54, result_line.str(), RGB(203, 213, 225));
    }

    for (const auto& job : app.snapshot.flatten_jobs) {
        const int cx = static_cast<int>(std::lround(WorldToScreenX(app, job.center.x, board)));
        const int cy = static_cast<int>(std::lround(WorldToScreenY(app, job.center.z, board)));
        const int radius = static_cast<int>(std::max(4.0f, job.radius * app.camera.zoom));
        const float progress = job.required_labor > 0.0f
            ? std::clamp(job.accumulated_labor / job.required_labor, 0.0f, 1.0f)
            : 1.0f;
        const bool ready_pad = progress >= 0.99f || job.state == "Completed";
        const COLORREF pad_color = ready_pad ? RGB(74, 222, 128)
            : (job.assigned_count > 0 ? RGB(250, 204, 21) : RGB(163, 230, 53));
        HPEN pen = CreatePen(PS_DASH, ready_pad ? 2 : 1, pad_color);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        std::wostringstream pad_label;
        pad_label << (ready_pad ? L"PAD READY " : L"PAD ")
                  << static_cast<int>(std::round(progress * 100.0f)) << L"%";
        if (job.assigned_count > 0) {
            pad_label << L" U" << job.assigned_count;
        }
        DrawLabel(hdc, cx + radius + 4, cy - radius, pad_label.str(), pad_color);
    }

    for (const auto& node : app.snapshot.resource_nodes) {
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, node.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, node.position.z, board)));
        const bool objective_iron_node = IsDepotRunObjectiveNode(app, node);
        COLORREF node_color = RGB(74, 222, 128);
        if (node.risk_band == "Medium") {
            node_color = RGB(250, 204, 21);
        } else if (node.risk_band == "High") {
            node_color = RGB(248, 113, 113);
        }
        if (objective_iron_node) {
            HPEN objective_pen = CreatePen(PS_DASH, 2, RGB(240, 249, 255));
            HGDIOBJ old_pen = SelectObject(hdc, objective_pen);
            HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Ellipse(hdc, sx - 15, sy - 15, sx + 15, sy + 15);
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(objective_pen);
        }
        HBRUSH brush = CreateSolidBrush(node_color);
        const int marker_radius = node.risk_band == "High" ? 7 : (node.risk_band == "Medium" ? 6 : 5);
        RECT rect {sx - marker_radius, sy - marker_radius, sx + marker_radius, sy + marker_radius};
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        DrawLabel(
            hdc,
            sx + marker_radius + 3,
            sy - marker_radius,
            std::wstring(objective_iron_node ? L"OBJ " : L"") +
                ToWide(node.risk_band.substr(0, 1)) + L" " + ToWide(node.produces_item_name) +
                L" x" + std::to_wstring(static_cast<int>(std::round(node.richness * node.extraction_rate * 100.0f))) + L"%",
            node_color);
    }

    for (const auto& storage : app.snapshot.storage_sites) {
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, storage.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, storage.position.z, board)));
        HBRUSH brush = CreateSolidBrush(RGB(34, 197, 94));
        RECT rect {sx - 8, sy - 8, sx + 8, sy + 8};
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }

    for (const auto& drop : app.snapshot.dropped_cargo) {
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, drop.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, drop.position.z, board)));
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(250, 204, 21));
        HBRUSH brush = CreateSolidBrush(RGB(168, 85, 247));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        POINT diamond[4] {
            {sx, sy - 8},
            {sx + 8, sy},
            {sx, sy + 8},
            {sx - 8, sy},
        };
        Polygon(hdc, diamond, 4);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    for (const auto& contact : app.snapshot.known_contacts) {
        if (contact.observing_player_id != app.primary_player_id) {
            continue;
        }
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, contact.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, contact.position.z, board)));
        const bool visible = contact.freshness == "visible";
        const KnownContactView* selected_contact = SelectedContact(app);
        const bool selected_contact_marker =
            selected_contact != nullptr &&
            selected_contact->target_entity_id == contact.target_entity_id &&
            selected_contact->target_kind == contact.target_kind;
        HPEN pen = CreatePen(
            visible ? PS_SOLID : PS_DOT,
            selected_contact_marker ? 3 : (visible ? 2 : 1),
            selected_contact_marker ? RGB(253, 224, 71) : (visible ? RGB(248, 113, 113) : RGB(148, 163, 184)));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, sx - 10, sy - 10, sx + 10, sy + 10);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        DrawLabel(
            hdc,
            sx + 10,
            sy + 8,
            selected_contact_marker ? L"SEL" : (visible ? L"VIS" : L"LAST"),
            selected_contact_marker ? RGB(253, 224, 71) : (visible ? RGB(248, 113, 113) : RGB(148, 163, 184)));
    }

    for (const auto& structure : app.snapshot.structures) {
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, structure.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, structure.position.z, board)));
        HBRUSH brush = CreateSolidBrush(RGB(56, 189, 248));
        RECT rect {sx - 7, sy - 7, sx + 7, sy + 7};
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        DrawHealthBar(hdc, sx - 14, sy + 11, 28, 4, structure.health, structure.max_health);
    }

    for (const auto& site : app.snapshot.construction_sites) {
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, site.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, site.position.z, board)));
        const int radius = static_cast<int>(std::max(8.0f, site.footprint_radius * app.camera.zoom * 0.45f));
        HPEN pen = CreatePen(PS_DASH, 2, RGB(251, 146, 60));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, sx - radius, sy - radius, sx + radius, sy + radius, 8, 8);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
        DrawHealthBar(hdc, sx - radius, sy - radius - 8, radius * 2, 5, site.health, site.max_health);
    }

    std::vector<POINT> queued_label_points;
    for (const auto& unit : app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const int sx = static_cast<int>(std::lround(WorldToScreenX(app, unit.position.x, board)));
        const int sy = static_cast<int>(std::lround(WorldToScreenY(app, unit.position.z, board)));
        const bool selected = IsSelected(app, unit.unit_id);
        const int radius = selected ? 8 : 6;
        HBRUSH brush = CreateSolidBrush(PlayerColor(unit.owner_player_id));
        COLORREF outline = RGB(17, 24, 39);
        if (selected) {
            outline = RGB(255, 255, 255);
        } else if (unit.order == "AttackTarget") {
            outline = RGB(248, 113, 113);
        } else if (unit.order == "Move" && unit.assignment_kind == 3 && unit.assignment_target_entity_id != 0) {
            outline = RGB(129, 140, 248);
        } else if (unit.order == "Escort") {
            outline = unit.assignment_target_entity_id == 0 ? RGB(52, 211, 153) : RGB(251, 191, 36);
        } else if (unit.max_health > 0.0f && unit.health / unit.max_health <= 0.35f) {
            outline = RGB(239, 68, 68);
        }
        HPEN pen = CreatePen(PS_SOLID, selected ? 3 : 2, outline);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        Ellipse(hdc, sx - radius, sy - radius, sx + radius, sy + radius);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(brush);
        DeleteObject(pen);
        DrawHealthBar(hdc, sx - 10, sy + radius + 3, 20, 4, unit.health, unit.max_health);
        if (selected && unit.queued_order_count > 0) {
            const int qx = static_cast<int>(std::lround(WorldToScreenX(app, unit.next_queued_target.x, board)));
            const int qy = static_cast<int>(std::lround(WorldToScreenY(app, unit.next_queued_target.z, board)));
            HPEN qpen = CreatePen(PS_DOT, 1, RGB(253, 224, 71));
            HGDIOBJ old_qpen = SelectObject(hdc, qpen);
            MoveToEx(hdc, sx, sy, nullptr);
            LineTo(hdc, qx, qy);
            SelectObject(hdc, old_qpen);
            DeleteObject(qpen);
            RECT qrect {qx - 4, qy - 4, qx + 4, qy + 4};
            HBRUSH qbrush = CreateSolidBrush(RGB(253, 224, 71));
            FillRect(hdc, &qrect, qbrush);
            DeleteObject(qbrush);
            bool label_nearby = false;
            for (const auto& point : queued_label_points) {
                const int dx = point.x - qx;
                const int dy = point.y - qy;
                if ((dx * dx) + (dy * dy) <= 256) {
                    label_nearby = true;
                    break;
                }
            }
            if (!label_nearby) {
                DrawLabel(hdc, qx + 6, qy - 6, L"Q", RGB(253, 224, 71));
                queued_label_points.push_back({qx, qy});
            }
        }
        if (IsDepotRunObjectiveCargo(unit)) {
            DrawLabel(hdc, sx + 9, sy - 28, L"IRON", RGB(96, 165, 250));
        }
        if (unit.order == "AttackTarget") {
            DrawLabel(hdc, sx + 9, sy - 14, L"ATK", RGB(248, 113, 113));
        } else if (unit.order == "Move" && unit.assignment_kind == 3 && unit.assignment_target_entity_id != 0) {
            DrawLabel(hdc, sx + 9, sy - 14, L"INT", RGB(129, 140, 248));
        } else if (unit.order == "Scout") {
            DrawLabel(hdc, sx + 9, sy - 14, unit.assignment_patrol_route ? L"PAT" : L"SCT", RGB(45, 212, 191));
        } else if (unit.order == "Escort") {
            DrawLabel(hdc, sx + 9, sy - 14, unit.assignment_target_entity_id == 0 ? L"HOLD" : L"GRD", RGB(251, 191, 36));
        }
    }

    if (app.dragging_selection) {
        RECT drag_rect {};
        drag_rect.left = std::min(app.drag_start_screen.x, app.drag_end_screen.x);
        drag_rect.right = std::max(app.drag_start_screen.x, app.drag_end_screen.x);
        drag_rect.top = std::min(app.drag_start_screen.y, app.drag_end_screen.y);
        drag_rect.bottom = std::max(app.drag_start_screen.y, app.drag_end_screen.y);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN pen = CreatePen(PS_DASH, 1, RGB(147, 197, 253));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        Rectangle(hdc, drag_rect.left, drag_rect.top, drag_rect.right, drag_rect.bottom);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
    }

    RECT panel = GetPanelRect(hwnd);
    HBRUSH panel_brush = CreateSolidBrush(RGB(17, 24, 39));
    FillRect(hdc, &panel, panel_brush);
    DeleteObject(panel_brush);
    FrameRect(hdc, &panel, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

    int py = panel.top + 12;
    DrawLabel(hdc, panel.left + 12, py, L"World Summary", RGB(240, 249, 255));
    py += 24;
    DrawLabel(hdc, panel.left + 12, py, L"Players: " + std::to_wstring(app.snapshot.players.size()));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Units: " + std::to_wstring(app.snapshot.units.size()));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Working: " + std::to_wstring(runtime_summary.working_units));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Combat: " + std::to_wstring(runtime_summary.combat_units));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Hauling: " + std::to_wstring(runtime_summary.hauling_units));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Retreating: " + std::to_wstring(runtime_summary.retreating_units));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Flatten Jobs: " + std::to_wstring(app.snapshot.flatten_jobs.size()));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Construction Sites: " + std::to_wstring(app.snapshot.construction_sites.size()));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Structures: " + std::to_wstring(app.snapshot.structures.size()));
    py += 30;
    DrawLabel(hdc, panel.left + 12, py, L"Dropped Cargo: " + std::to_wstring(app.snapshot.dropped_cargo.size()), RGB(250, 204, 21));
    py += 18;
    for (std::size_t i = 0; i < std::min<std::size_t>(app.snapshot.dropped_cargo.size(), 3); ++i) {
        const auto& drop = app.snapshot.dropped_cargo[i];
        std::wostringstream drop_line;
        drop_line << L"  #" << drop.dropped_cargo_id << L" " << ToWide(drop.primary_item_name)
                  << L" stacks " << drop.stack_count
                  << L" wt " << std::fixed << std::setprecision(1) << drop.total_weight;
        DrawLabel(hdc, panel.left + 12, py, drop_line.str(), RGB(250, 204, 21));
        py += 18;
    }
    std::size_t visible_contacts = 0;
    std::size_t stale_contacts = 0;
    for (const auto& contact : app.snapshot.known_contacts) {
        if (contact.observing_player_id != app.primary_player_id) {
            continue;
        }
        if (contact.freshness == "visible") {
            ++visible_contacts;
        } else {
            ++stale_contacts;
        }
    }
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        L"Contacts: visible " + std::to_wstring(visible_contacts) + L" / stale " + std::to_wstring(stale_contacts),
        visible_contacts > 0 ? RGB(248, 113, 113) : RGB(148, 163, 184));
    py += 18;
    if (const KnownContactView* contact = SelectedContact(app); contact != nullptr) {
        std::wostringstream contact_line;
        contact_line << L"Selected Contact: #" << contact->target_entity_id
                     << L" " << ToWide(contact->target_kind)
                     << L" " << ToWide(contact->freshness);
        DrawLabel(hdc, panel.left + 12, py, contact_line.str(), contact->freshness == "visible" ? RGB(248, 113, 113) : RGB(253, 224, 71));
        py += 18;
        std::wostringstream seen_line;
        seen_line << L"  last tick " << contact->last_seen_tick
                  << L" by #" << contact->spotted_by_unit_id
                  << L" at " << static_cast<int>(std::round(contact->position.x))
                  << L"," << static_cast<int>(std::round(contact->position.z));
        DrawLabel(hdc, panel.left + 12, py, seen_line.str(), RGB(148, 163, 184));
        py += 18;
    } else if (visible_contacts + stale_contacts > 0) {
        DrawLabel(hdc, panel.left + 12, py, L"Selected Contact: none (N cycles)", RGB(148, 163, 184));
        py += 18;
    }
    py += 12;
    std::wstring group_line = L"Groups:";
    for (std::size_t i = 0; i < app.control_groups.size(); ++i) {
        group_line += L" ";
        group_line += std::to_wstring(i + 1);
        group_line += L"[";
        group_line += std::to_wstring(app.control_groups[i].size());
        group_line += L"]";
    }
    DrawLabel(hdc, panel.left + 12, py, group_line, RGB(196, 181, 253));
    py += 24;
    std::size_t selected_short = 0;
    std::size_t selected_mid = 0;
    std::size_t selected_long = 0;
    std::size_t selected_overburdened = 0;
    std::size_t selected_low_health = 0;
    std::size_t selected_in_combat = 0;
    std::size_t selected_retreating_now = 0;
    std::size_t selected_fast_retreat_ready = 0;
    float selected_cargo_weight = 0.0f;
    float selected_carry_capacity = 0.0f;
    float selected_stamina_total = 0.0f;
    float selected_stamina_capacity = 0.0f;
    float nearest_depot_distance_sq = 0.0f;
    bool has_nearest_depot = false;
    for (Id unit_id : app.selected_units) {
        const UnitView* unit = FindUnitById(app, unit_id);
        if (unit == nullptr) {
            continue;
        }
        if (unit->combat_band == "Short") {
            ++selected_short;
        } else if (unit->combat_band == "Long") {
            ++selected_long;
        } else {
            ++selected_mid;
        }
        selected_cargo_weight += unit->cargo_weight;
        selected_carry_capacity += unit->carry_capacity;
        selected_stamina_total += unit->stamina;
        selected_stamina_capacity += unit->max_stamina;
        if (unit->cargo_weight > unit->carry_capacity) {
            ++selected_overburdened;
        }
        if (unit->max_health > 0.0f && unit->health / unit->max_health <= 0.35f) {
            ++selected_low_health;
        }
        if (unit->order == "AttackTarget") {
            ++selected_in_combat;
        }
        if (unit->order == "Retreat") {
            ++selected_retreating_now;
        }
        if (unit->stamina >= unit->max_stamina * 0.45f && unit->cargo_weight <= unit->carry_capacity) {
            ++selected_fast_retreat_ready;
        }
        for (const auto& storage : app.snapshot.storage_sites) {
            if (storage.owner_player_id != unit->owner_player_id) {
                continue;
            }
            const float distance_sq = ViewDistanceSquared(unit->position, storage.position);
            if (!has_nearest_depot || distance_sq < nearest_depot_distance_sq) {
                nearest_depot_distance_sq = distance_sq;
                has_nearest_depot = true;
            }
        }
    }
    std::wostringstream cargo_risk;
    cargo_risk << L"Selected cargo risk: " << std::fixed << std::setprecision(1) << selected_cargo_weight
               << L"/" << selected_carry_capacity;
    if (selected_overburdened > 0) {
        cargo_risk << L"  overloaded " << selected_overburdened;
    }
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        L"Selected bands: S " + std::to_wstring(selected_short) +
            L" M " + std::to_wstring(selected_mid) +
            L" L " + std::to_wstring(selected_long),
        RGB(191, 219, 254));
    py += 18;
    std::wstring role_hint = L"Band plan: ";
    if (selected_short > 0 && selected_long > 0) {
        role_hint += L"Short pins, Long covers";
    } else if (selected_short > 0 && selected_mid == 0 && selected_long == 0) {
        role_hint += L"commit close or disengage";
    } else if (selected_long > 0 && selected_short == 0) {
        role_hint += L"kite/screen, avoid being pinned";
    } else {
        role_hint += L"balanced skirmish";
    }
    DrawLabel(hdc, panel.left + 12, py, role_hint, RGB(148, 163, 184));
    py += 18;
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        cargo_risk.str(),
        selected_overburdened > 0 ? RGB(248, 113, 113) : RGB(148, 163, 184));
    py += 18;
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        L"Selected danger: lowHP " + std::to_wstring(selected_low_health) +
            L" combat " + std::to_wstring(selected_in_combat),
        (selected_low_health + selected_in_combat) > 0 ? RGB(251, 191, 36) : RGB(148, 163, 184));
    py += 18;
    const bool can_retreat_cleanly =
        !app.selected_units.empty() &&
        selected_fast_retreat_ready >= std::max<std::size_t>(1, app.selected_units.size() / 2) &&
        selected_overburdened == 0;
    std::wostringstream retreat_line;
    retreat_line << L"Retreat read: "
                 << (can_retreat_cleanly ? L"clean" : L"risky")
                 << L" / ready " << selected_fast_retreat_ready
                 << L" / retreating " << selected_retreating_now;
    if (selected_stamina_capacity > 0.0f) {
        retreat_line << L" / STA " << static_cast<int>(std::round((selected_stamina_total / selected_stamina_capacity) * 100.0f)) << L"%";
    }
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        retreat_line.str(),
        can_retreat_cleanly ? RGB(74, 222, 128) : RGB(251, 191, 36));
    py += 18;
    if (has_nearest_depot) {
        std::wostringstream depot_line;
        depot_line << L"Nearest depot: " << std::fixed << std::setprecision(1) << std::sqrt(nearest_depot_distance_sq) << L"m";
        DrawLabel(hdc, panel.left + 12, py, depot_line.str(), RGB(148, 163, 184));
        py += 18;
    }
    py += 6;
    DrawLabel(hdc, panel.left + 12, py, L"Session", RGB(240, 249, 255));
    py += 24;
    if (app.session.has_scenario) {
        const bool scenario_failed = app.session.scenario.state == "failed";
        const bool scenario_success = app.session.scenario.state == "success";
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            ToWide(app.session.scenario.name) + L": " + ToWide(app.session.scenario.state),
            scenario_success ? RGB(74, 222, 128) : (scenario_failed ? RGB(248, 113, 113) : RGB(250, 204, 21)));
        py += 18;
        DrawLabel(hdc, panel.left + 12, py, ToWide(app.session.scenario.summary), RGB(191, 219, 254));
        py += 18;
        DrawLabel(hdc, panel.left + 12, py, L"Reason: " + ToWide(app.session.scenario.reason), scenario_failed ? RGB(248, 113, 113) : RGB(148, 163, 184));
        py += 18;
        std::wostringstream scenario_line;
        scenario_line << L"Iron "
                      << static_cast<int>(std::round(app.session.scenario.iron_fitting_stored))
                      << L"/" << static_cast<int>(std::round(app.session.scenario.iron_fitting_goal))
                      << L"  Node #"
                      << app.session.scenario.iron_node_id
                      << L"  Forward depot "
                      << (app.session.scenario.forward_depot_complete ? L"done" : L"open")
                      << L"  Time "
                      << app.session.scenario.remaining_ticks << L"/" << app.session.scenario.time_limit_ticks;
        DrawLabel(hdc, panel.left + 12, py, scenario_line.str(), RGB(148, 163, 184));
        py += 18;
        std::wostringstream result_line;
        result_line << L"Result read: lost " << app.session.scenario.units_lost
                    << L" / depot HP "
                    << static_cast<int>(std::round(app.session.scenario.primary_depot_health))
                    << L"/"
                    << static_cast<int>(std::round(app.session.scenario.primary_depot_max_health));
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            result_line.str(),
            app.session.scenario.units_lost > 0 || scenario_failed ? RGB(251, 191, 36) : RGB(148, 163, 184));
        py += 24;
    }
    const std::wstring last_command_id = app.command_feedback.loaded ? ToWide(app.command_feedback.command_id) : L"(none)";
    const std::wstring last_command_status = app.command_feedback.loaded
        ? ToWide(app.command_feedback.status)
        : ToWide(app.session.last_command_ok ? "OK" : "Pending/Fail");
    const std::wstring last_command_message = app.command_feedback.loaded
        ? ToWide(app.command_feedback.message)
        : ToWide(app.session.last_command_message.empty() ? "No command yet." : app.session.last_command_message);
    DrawLabel(hdc, panel.left + 12, py, L"Last Command: " + last_command_id);
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Status: " + last_command_status);
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, last_command_message, RGB(148, 163, 184));
    py += 30;
    DrawLabel(hdc, panel.left + 12, py, L"Economy", RGB(240, 249, 255));
    py += 24;
    const PlayerView* primary_player = FindPlayerById(app, app.primary_player_id);
    if (primary_player != nullptr) {
        DrawLabel(hdc, panel.left + 12, py, L"Credits: " + std::to_wstring(static_cast<int>(std::round(primary_player->credits))), RGB(191, 219, 254));
        py += 18;
        std::wostringstream load_line;
        load_line << L"Upkeep " << static_cast<int>(std::round(primary_player->upkeep_load))
                  << L"  Food " << static_cast<int>(std::round(primary_player->food_load))
                  << L"  Tax " << static_cast<int>(std::round(primary_player->tax_load));
        DrawLabel(hdc, panel.left + 12, py, load_line.str(), RGB(148, 163, 184));
        py += 18;
        std::wostringstream pressure_line;
        pressure_line << L"Efficiency " << static_cast<int>(std::round(primary_player->efficiency * 100.0f))
                      << L"%  Pressure " << static_cast<int>(std::round(primary_player->pressure_ratio * 100.0f))
                      << L"%  Complexity " << static_cast<int>(std::round(primary_player->complexity_load));
        DrawLabel(hdc, panel.left + 12, py, pressure_line.str(), RGB(148, 163, 184));
        py += 18;
        std::wostringstream shortage_line;
        shortage_line << L"Food shortage " << static_cast<int>(std::round(primary_player->food_shortage * 100.0f))
                      << L"%  Credit shortage " << static_cast<int>(std::round(primary_player->upkeep_shortage * 100.0f))
                      << L"%";
        DrawLabel(hdc, panel.left + 12, py, shortage_line.str(), RGB(148, 163, 184));
        py += 18;
    }
    if (app.session.has_economy) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Pressure band: " + PressureBandLabel(app.session.economy.pressure_band),
            app.session.economy.pressure_band == "high" ? RGB(248, 113, 113)
                : (app.session.economy.pressure_band == "medium" ? RGB(251, 191, 36) : RGB(74, 222, 128)));
        py += 18;
    }
    if (!app.session.runtime_alert.empty()) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Runtime alert: " + ToWide(app.session.runtime_alert),
            RGB(248, 113, 113));
        py += 18;
    }
    if (!app.session.supply_alert.empty()) {
        const bool supply_critical = app.session.supply_alert.find("critical") != std::string::npos ||
            app.session.supply_alert.find("threatened") != std::string::npos;
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Supply: " + ToWide(app.session.supply_alert),
            supply_critical ? RGB(248, 113, 113) : RGB(74, 222, 128));
        py += 18;
    }
    if (!app.session.repair_alert.empty()) {
        const bool repair_bad = app.session.repair_alert.find("stalled") != std::string::npos ||
            app.session.repair_alert.find("low") != std::string::npos ||
            app.session.repair_alert.find("Critical") != std::string::npos;
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Repair: " + ToWide(app.session.repair_alert),
            repair_bad ? RGB(251, 191, 36) : RGB(74, 222, 128));
        py += 18;
    }
    Id top_route_hauler_id = 0;
    Id top_route_enemy_id = 0;
    std::string top_route_level {};
    int top_route_rank = 0;
    for (const auto& unit : app.snapshot.units) {
        if (!unit.route_active || unit.owner_player_id != app.primary_player_id) {
            continue;
        }
        const int rank = unit.route_threat_level == "critical" ? 2 : (unit.route_threat_level == "threatened" ? 1 : 0);
        if (rank > top_route_rank) {
            top_route_rank = rank;
            top_route_hauler_id = unit.unit_id;
            top_route_enemy_id = unit.route_threat_enemy_id;
            top_route_level = unit.route_threat_level;
        }
    }
    if (top_route_hauler_id != 0) {
        std::wostringstream route_line;
        route_line << L"Route threat: hauler #" << top_route_hauler_id
                   << L" " << ToWide(top_route_level)
                   << L" enemy #" << top_route_enemy_id
                   << L"  K guard / Shift+K intercept";
        DrawLabel(hdc, panel.left + 12, py, route_line.str(), top_route_rank >= 2 ? RGB(248, 113, 113) : RGB(251, 191, 36));
        py += 18;
    }
    if (!app.session.combat_summary.empty()) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Combat: " + ToWide(app.session.combat_summary),
            app.session.combat_summary == "No active combat pressure." ? RGB(148, 163, 184) : RGB(251, 191, 36));
        py += 18;
    }
    if (!app.session.combat_alert.empty()) {
        const bool retreat_alert = app.session.combat_alert.find("Retreat") != std::string::npos ||
            app.session.combat_alert.find("critical") != std::string::npos;
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Combat alert: " + ToWide(app.session.combat_alert),
            retreat_alert ? RGB(248, 113, 113) : RGB(251, 191, 36));
        py += 18;
    }
    if (!app.session.contact_summary.empty()) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            ToWide(app.session.contact_summary),
            app.session.contact_summary.find("visible 0") == std::string::npos ? RGB(248, 113, 113) : RGB(148, 163, 184));
        py += 18;
    }
    if (!app.session.contact_alert.empty()) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Contact alert: " + ToWide(app.session.contact_alert),
            RGB(253, 224, 71));
        py += 18;
    }
    bool depot_damaged = false;
    bool depot_missing = primary_player != nullptr;
    for (const auto& storage : app.snapshot.storage_sites) {
        if (primary_player != nullptr && storage.owner_player_id == primary_player->player_id) {
            depot_missing = false;
            if (storage.stored_stack_count == 0) {
                depot_damaged = depot_damaged || false;
            }
        }
    }
    for (const auto& structure : app.snapshot.structures) {
        if (primary_player != nullptr &&
            structure.owner_player_id == primary_player->player_id &&
            structure.structure_type == "StorageDepot") {
            depot_missing = false;
            if (structure.health < structure.max_health) {
                depot_damaged = true;
            }
        }
    }
    if (depot_missing) {
        DrawLabel(hdc, panel.left + 12, py, L"Alert: primary depot lost; stored resources were dropped or destroyed.", RGB(248, 113, 113));
        py += 18;
    } else if (depot_damaged) {
        DrawLabel(hdc, panel.left + 12, py, L"Alert: primary depot under attack or recently damaged.", RGB(251, 191, 36));
        py += 18;
    }
    int objective_carriers = 0;
    int threatened_objective_carriers = 0;
    for (const auto& unit : app.snapshot.units) {
        if (!unit.alive || primary_player == nullptr || unit.owner_player_id != primary_player->player_id || !IsDepotRunObjectiveCargo(unit)) {
            continue;
        }
        ++objective_carriers;
        if (IsObjectiveCargoThreatened(app, unit)) {
            ++threatened_objective_carriers;
        }
    }
    if (objective_carriers > 0) {
        std::wostringstream objective_risk_line;
        objective_risk_line << L"Objective cargo: " << objective_carriers << L" carrier(s)";
        if (threatened_objective_carriers > 0) {
            objective_risk_line << L" / threatened " << threatened_objective_carriers << L" - guard or deposit now";
        } else {
            objective_risk_line << L" / safe enough to bank";
        }
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            objective_risk_line.str(),
            threatened_objective_carriers > 0 ? RGB(248, 113, 113) : RGB(96, 165, 250));
        py += 18;
    }
    bool construction_exposed = false;
    bool midfield_contested = false;
    bool build_pad_contested = false;
    for (const auto& site : app.snapshot.construction_sites) {
        if (primary_player == nullptr || site.owner_player_id != primary_player->player_id) {
            continue;
        }
        int friendly_near = 0;
        int enemy_near = 0;
        constexpr float kConstructionReadRadiusSq = 28.0f * 28.0f;
        for (const auto& unit : app.snapshot.units) {
            if (!unit.alive || ViewDistanceSquared(unit.position, site.position) > kConstructionReadRadiusSq) {
                continue;
            }
            if (unit.owner_player_id == primary_player->player_id) {
                ++friendly_near;
            } else {
                ++enemy_near;
            }
        }
        construction_exposed = construction_exposed || (enemy_near > 0 && friendly_near < 2);
        midfield_contested = midfield_contested || (enemy_near > 0);
    }
    for (const auto& job : app.snapshot.flatten_jobs) {
        int enemy_near = 0;
        constexpr float kPadReadRadiusSq = 24.0f * 24.0f;
        for (const auto& unit : app.snapshot.units) {
            if (!unit.alive || primary_player == nullptr || unit.owner_player_id == primary_player->player_id) {
                continue;
            }
            if (ViewDistanceSquared(unit.position, job.center) <= kPadReadRadiusSq) {
                ++enemy_near;
            }
        }
        build_pad_contested = build_pad_contested || (enemy_near > 0 && job.assigned_count > 0);
        midfield_contested = midfield_contested || (enemy_near > 0 && job.assigned_count > 0);
    }
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        std::wstring(L"Depot: ") + (depot_missing ? L"lost" : (depot_damaged ? L"damaged" : L"safe")),
        depot_missing ? RGB(248, 113, 113) : (depot_damaged ? RGB(251, 191, 36) : RGB(74, 222, 128)));
    py += 18;
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        std::wstring(L"Mid-field: ") + (midfield_contested ? L"contested" : L"quiet"),
        midfield_contested ? RGB(251, 191, 36) : RGB(148, 163, 184));
    py += 18;
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        std::wstring(L"Construction: ") + (construction_exposed ? L"exposed" : L"covered"),
        construction_exposed ? RGB(248, 113, 113) : RGB(148, 163, 184));
    py += 18;
    DrawLabel(
        hdc,
        panel.left + 12,
        py,
        std::wstring(L"Build pad pressure: ") + (build_pad_contested ? L"workers threatened" : L"quiet"),
        build_pad_contested ? RGB(248, 113, 113) : RGB(148, 163, 184));
    py += 18;
    const StorageSiteView* primary_storage = nullptr;
    if (primary_player != nullptr) {
        for (const auto& storage : app.snapshot.storage_sites) {
            if (storage.owner_player_id == primary_player->player_id) {
                primary_storage = &storage;
                break;
            }
        }
    }
    if (primary_storage != nullptr) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Depot stock: " + ToWide(primary_storage->primary_item_name) +
                L" / stacks " + std::to_wstring(primary_storage->stored_stack_count),
            primary_storage->stored_stack_count > 0 ? RGB(191, 219, 254) : RGB(148, 163, 184));
        py += 18;
    }
    int low_nodes = 0;
    int mid_nodes = 0;
    int high_nodes = 0;
    const ResourceNodeView* best_node = nullptr;
    float best_score = 0.0f;
    for (const auto& node : app.snapshot.resource_nodes) {
        if (node.risk_band == "High") {
            ++high_nodes;
        } else if (node.risk_band == "Medium") {
            ++mid_nodes;
        } else {
            ++low_nodes;
        }
        const float score = node.richness * node.extraction_rate;
        if (best_node == nullptr || score > best_score) {
            best_node = &node;
            best_score = score;
        }
    }
    if (best_node != nullptr) {
        std::wostringstream resource_line;
        resource_line << L"Resources: safe " << low_nodes
                      << L" / mid " << mid_nodes
                      << L" / high " << high_nodes
                      << L" | best #" << best_node->resource_node_id
                      << L" " << ToWide(best_node->risk_band)
                      << L" " << ToWide(best_node->produces_item_name)
                      << L" x" << static_cast<int>(std::round(best_score * 100.0f)) << L"%";
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            resource_line.str(),
            best_node->risk_band == "High" ? RGB(248, 113, 113)
                : (best_node->risk_band == "Medium" ? RGB(250, 204, 21) : RGB(74, 222, 128)));
        py += 18;
    }
    int build_pads_ready = 0;
    int build_pads_active = 0;
    int build_pads_exposed = 0;
    for (const auto& job : app.snapshot.flatten_jobs) {
        const float progress = job.required_labor > 0.0f
            ? std::clamp(job.accumulated_labor / job.required_labor, 0.0f, 1.0f)
            : 1.0f;
        if (progress >= 0.99f || job.state == "Completed") {
            ++build_pads_ready;
        }
        if (job.assigned_count > 0) {
            ++build_pads_active;
        }
        int enemies_near = 0;
        constexpr float kPadThreatRadiusSq = 24.0f * 24.0f;
        for (const auto& unit : app.snapshot.units) {
            if (!unit.alive || primary_player == nullptr || unit.owner_player_id == primary_player->player_id) {
                continue;
            }
            if (ViewDistanceSquared(unit.position, job.center) <= kPadThreatRadiusSq) {
                ++enemies_near;
            }
        }
        if (enemies_near > 0) {
            ++build_pads_exposed;
        }
    }
    if (!app.snapshot.flatten_jobs.empty()) {
        std::wostringstream pad_line;
        pad_line << L"Build pads: " << build_pads_ready << L" ready / "
                 << build_pads_active << L" active / "
                 << build_pads_exposed << L" exposed";
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            pad_line.str(),
            build_pads_exposed > 0 ? RGB(248, 113, 113)
                : (build_pads_ready > 0 ? RGB(74, 222, 128) : RGB(250, 204, 21)));
        py += 18;
    }
    int selected_queued = 0;
    int selected_fighting = 0;
    int selected_retreating = 0;
    int selected_scouting = 0;
    int selected_hauling = 0;
    int selected_threatened_haulers = 0;
    int selected_escorts = 0;
    int selected_objective_cargo = 0;
    for (const auto& unit : app.snapshot.units) {
        if (!IsSelected(app, unit.unit_id)) {
            continue;
        }
        selected_queued += unit.queued_order_count > 0 ? 1 : 0;
        selected_fighting += unit.order == "AttackTarget" ? 1 : 0;
        selected_retreating += unit.order == "Retreat" ? 1 : 0;
        selected_scouting += unit.order == "Scout" ? 1 : 0;
        selected_hauling += unit.route_active ? 1 : 0;
        selected_threatened_haulers += unit.route_active && unit.route_threat_level != "safe" ? 1 : 0;
        selected_escorts += unit.order == "Escort" ? 1 : 0;
        selected_objective_cargo += IsDepotRunObjectiveCargo(unit) ? 1 : 0;
    }
    if (!app.selected_units.empty()) {
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Group: queued " + std::to_wstring(selected_queued) +
                L" / fighting " + std::to_wstring(selected_fighting) +
                L" / retreating " + std::to_wstring(selected_retreating) +
                L" / scouting " + std::to_wstring(selected_scouting),
            RGB(191, 219, 254));
        py += 18;
        DrawLabel(
            hdc,
            panel.left + 12,
            py,
            L"Supply group: haulers " + std::to_wstring(selected_hauling) +
                L" / threatened " + std::to_wstring(selected_threatened_haulers) +
                L" / escorts " + std::to_wstring(selected_escorts),
            selected_threatened_haulers > 0 ? RGB(248, 113, 113) : RGB(148, 163, 184));
        py += 18;
        if (app.session.has_scenario) {
            DrawLabel(
                hdc,
                panel.left + 12,
                py,
                L"Objective cargo: Iron carriers " + std::to_wstring(selected_objective_cargo),
                selected_objective_cargo > 0 ? RGB(96, 165, 250) : RGB(148, 163, 184));
            py += 18;
        }
    }
    py += 12;
    DrawLabel(hdc, panel.left + 12, py, L"Selected Units", RGB(240, 249, 255));
    py += 24;

    int shown = 0;
    for (const auto& unit : app.snapshot.units) {
        if (!IsSelected(app, unit.unit_id)) {
            continue;
        }
        std::wstring line = L"#" + std::to_wstring(unit.unit_id)
            + L" " + ToWide(unit.order)
            + L" " + CombatBandLabel(unit.combat_band)
            + L" HP " + std::to_wstring(static_cast<int>(std::round(unit.health)))
            + L" STA " + std::to_wstring(static_cast<int>(std::round(unit.stamina)))
            + L" Cargo " + std::to_wstring(static_cast<int>(std::round(unit.cargo_weight)))
            + L" " + ToWide(unit.cargo_primary_item_name)
            + L" VIS " + std::to_wstring(static_cast<int>(std::round(unit.vision_range)))
            + L" Target " + std::to_wstring(unit.assignment_target_entity_id);
        COLORREF unit_color = RGB(191, 219, 254);
        if (unit.max_health > 0.0f && unit.health / unit.max_health <= 0.35f) {
            unit_color = RGB(248, 113, 113);
        } else if (unit.order == "AttackTarget") {
            unit_color = RGB(251, 146, 60);
        } else if (unit.order == "Escort") {
            unit_color = RGB(251, 191, 36);
        }
        DrawLabel(hdc, panel.left + 12, py, line, unit_color);
        py += 18;
        std::wstring tactical = L"  ";
        if (IsDepotRunObjectiveCargo(unit)) {
            tactical += L"Objective cargo: Iron Fitting / ";
        }
        if (unit.order == "AttackTarget") {
            tactical += L"Focusing target #" + std::to_wstring(unit.assignment_target_entity_id);
            if (const auto target_position = AttackTargetPosition(app, unit); target_position.has_value()) {
                const float distance = std::sqrt(ViewDistanceSquared(unit.position, *target_position));
                const float range = CombatRangeForBand(unit.combat_band);
                tactical += L" / ";
                tactical += distance <= range ? L"in range " : L"closing ";
                tactical += std::to_wstring(static_cast<int>(std::round(distance)));
                tactical += L"/";
                tactical += std::to_wstring(static_cast<int>(std::round(range)));
            }
        } else if (unit.order == "Retreat") {
            tactical += L"Retreating";
        } else if (unit.order == "Move" && unit.assignment_kind == 3 && unit.assignment_target_entity_id != 0) {
            tactical += L"Intercepting target #" + std::to_wstring(unit.assignment_target_entity_id);
        } else if (unit.order == "Scout" && unit.assignment_patrol_route) {
            tactical += L"Patrolling route";
        } else if (unit.order == "Scout") {
            tactical += L"Scouting area";
        } else if (unit.order == "Escort" && unit.assignment_target_entity_id != 0) {
            const UnitView* guarded_unit = FindUnitById(app, unit.assignment_target_entity_id);
            if (guarded_unit != nullptr && guarded_unit->route_active) {
                tactical += L"Escorting hauler #" + std::to_wstring(unit.assignment_target_entity_id);
            } else {
                tactical += L"Guarding target #" + std::to_wstring(unit.assignment_target_entity_id);
            }
        } else if (unit.order == "Escort") {
            tactical += L"Holding position";
        } else if (unit.route_active) {
            tactical += L"Hauling ";
            tactical += ToWide(unit.route_source_kind);
            tactical += L" #";
            tactical += std::to_wstring(unit.route_source_id);
            tactical += L" -> depot #";
            tactical += std::to_wstring(unit.route_storage_id);
            tactical += L" / ";
            tactical += ToWide(unit.route_phase);
        } else {
            tactical += L"Ready";
        }
        if (!unit.queue_interrupt_reason.empty()) {
            tactical += L" / ";
            tactical += ToWide(unit.queue_interrupt_reason);
        } else if (!unit.tactical_state.empty()) {
            tactical += L" / ";
            tactical += ToWide(unit.tactical_state);
        }
        if (unit.route_active) {
            tactical += L" / Route ";
            tactical += ToWide(unit.route_threat_level.empty() ? "safe" : unit.route_threat_level);
            if (unit.route_threat_enemy_id != 0) {
                tactical += L" #";
                tactical += std::to_wstring(unit.route_threat_enemy_id);
            }
            if (unit.route_threat_level == "threatened" || unit.route_threat_level == "critical") {
                tactical += L" / K guard Shift+K intercept Y deposit U drop E retreat";
            }
        }
        if (unit.queued_order_count > 0) {
            tactical += L" / Queued ";
            tactical += std::to_wstring(unit.queued_order_count);
            tactical += L"/2 ";
            tactical += ToWide(unit.next_queued_order);
            tactical += L" @";
            tactical += std::to_wstring(static_cast<int>(std::round(unit.next_queued_target.x)));
            tactical += L",";
            tactical += std::to_wstring(static_cast<int>(std::round(unit.next_queued_target.z)));
        }
        if (unit.max_health > 0.0f && unit.health / unit.max_health <= 0.35f) {
            tactical += unit.cargo_weight > unit.carry_capacity ? L" / Retreat risky" : L" / Retreat now";
        } else if (unit.cargo_weight > unit.carry_capacity && unit.order == "AttackTarget") {
            tactical += L" / overloaded chase";
        }
        DrawLabel(hdc, panel.left + 12, py, tactical, unit_color);
        py += 18;
        if (++shown >= 10) {
            break;
        }
    }
    if (shown == 0) {
        DrawLabel(hdc, panel.left + 12, py, L"No units selected.", RGB(148, 163, 184));
        py += 18;
    } else {
        py += 12;
    }

    DrawLabel(hdc, panel.left + 12, py, L"Automation Editor", RGB(240, 249, 255));
    py += 22;
    DrawLabel(hdc, panel.left + 12, py, L"Scope: " + AutomationScopeName(app.automation_editor.scope), RGB(196, 181, 253));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Target: " + ToWide(app.automation_editor.source_label), RGB(191, 219, 254));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Preset: " + MatchingAutomationPresetName(app.automation_editor.working_rules), RGB(148, 163, 184));
    py += 18;
    if (app.automation_editor.mixed_source_rules) {
        DrawLabel(hdc, panel.left + 12, py, L"Source rules differ; editing starts from the first target.", RGB(251, 191, 36));
        py += 18;
    }
    if (app.automation_editor.scope == AutomationScope::Squad) {
        const std::wstring squad_hint = app.automation_editor.target_squad_id != 0
            ? L"Squad Id: " + std::to_wstring(app.automation_editor.target_squad_id)
            : L"Squad Id: (none)";
        DrawLabel(hdc, panel.left + 12, py, squad_hint, RGB(148, 163, 184));
        py += 18;
    }

    int shown_rules = 0;
    for (std::size_t i = 0; i < app.automation_editor.working_rules.size() && shown_rules < 4; ++i) {
        const auto& rule = app.automation_editor.working_rules[i];
        const std::wstring prefix = i == app.automation_editor.selected_rule_index ? L"> " : L"  ";
        std::wostringstream line;
        line << prefix << (i + 1) << L". "
             << ToWide(AutomationTriggerName(rule.trigger))
             << L" -> " << ToWide(AutomationActionName(rule.action))
             << L" @ " << std::fixed << std::setprecision(2) << rule.threshold
             << (rule.enabled ? L" on" : L" off");
        DrawLabel(hdc, panel.left + 12, py, line.str(), i == app.automation_editor.selected_rule_index ? RGB(253, 224, 71) : RGB(148, 163, 184));
        py += 18;
        ++shown_rules;
    }
    if (shown_rules == 0) {
        DrawLabel(hdc, panel.left + 12, py, L"No rules loaded. Press Insert to add one.", RGB(148, 163, 184));
        py += 18;
    }
    DrawLabel(hdc, panel.left + 12, py, app.automation_editor.dirty ? L"Pending apply" : L"In sync with latest snapshot", app.automation_editor.dirty ? RGB(248, 113, 113) : RGB(74, 222, 128));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"F5 scope  F6 squad  Ins add  Del remove  Enter apply", RGB(248, 250, 252));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Up/Down rule  Ctrl+Z trigger  X action  C enabled  Left/Right threshold", RGB(148, 163, 184));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"F1 Gather  F2 Escort  F3 Scout  F4 Hold", RGB(191, 219, 254));
    py += 18;
    DrawLabel(hdc, panel.left + 12, py, L"Build risk: sites auto-build at fixed speed; attacks only reduce site HP.", RGB(148, 163, 184));
    py += 24;

    if (!app.snapshot.construction_sites.empty()) {
        DrawLabel(hdc, panel.left + 12, py, L"Construction", RGB(240, 249, 255));
        py += 24;
        int shown_sites = 0;
        for (const auto& site : app.snapshot.construction_sites) {
            std::wstring site_line = L"#" + std::to_wstring(site.construction_site_id)
                + L" " + ToWide(site.structure_type)
                + L" " + ToWide(site.stage)
                + L" HP " + std::to_wstring(static_cast<int>(std::round(site.health)))
                + L" Progress " + std::to_wstring(static_cast<int>(std::round(site.completion_ratio * 100.0f))) + L"%";
            DrawLabel(hdc, panel.left + 12, py, site_line, RGB(253, 186, 116));
            py += 18;
            if (++shown_sites >= 5) {
                break;
            }
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_CREATE:
            SetTimer(hwnd, 1, kRefreshIntervalMs, nullptr);
            return 0;
        case WM_TIMER:
            if (g_app != nullptr) {
                RefreshSnapshot(*g_app);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (g_app != nullptr) {
                const short delta = GET_WHEEL_DELTA_WPARAM(w_param);
                g_app->camera.zoom = std::clamp(g_app->camera.zoom + (delta > 0 ? 0.5f : -0.5f), 2.0f, 18.0f);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MBUTTONDOWN:
            if (g_app != nullptr) {
                g_app->panning_camera = true;
                g_app->pan_anchor_screen = {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                g_app->pan_anchor_camera = g_app->camera;
                SetCapture(hwnd);
            }
            return 0;
        case WM_MBUTTONUP:
            if (g_app != nullptr && g_app->panning_camera) {
                g_app->panning_camera = false;
                ReleaseCapture();
            }
            return 0;
        case WM_MOUSEMOVE:
            if (g_app != nullptr && g_app->panning_camera) {
                const POINT current {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                g_app->camera.center_x = g_app->pan_anchor_camera.center_x - (static_cast<float>(current.x - g_app->pan_anchor_screen.x) / g_app->camera.zoom);
                g_app->camera.center_z = g_app->pan_anchor_camera.center_z - (static_cast<float>(current.y - g_app->pan_anchor_screen.y) / g_app->camera.zoom);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (g_app != nullptr && g_app->dragging_selection) {
                g_app->drag_end_screen = {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (g_app != nullptr) {
                RECT board = GetBoardRect(hwnd);
                const POINT point {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                if (PtInRect(&board, point)) {
                    g_app->dragging_selection = true;
                    g_app->drag_start_screen = point;
                    g_app->drag_end_screen = point;
                    SetCapture(hwnd);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (g_app != nullptr && g_app->dragging_selection) {
                RECT board = GetBoardRect(hwnd);
                const POINT point {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                const bool additive = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const int drag_dx = std::abs(point.x - g_app->drag_start_screen.x);
                const int drag_dy = std::abs(point.y - g_app->drag_start_screen.y);

                if (drag_dx <= 4 && drag_dy <= 4) {
                    if (g_app->command_mode == CommandMode::Move) {
                        if (auto unit = FindUnitAtPoint(*g_app, board, point); unit.has_value()) {
                            if (CanSelectUnit(*g_app, *unit)) {
                                if (!additive) {
                                    g_app->selected_units.clear();
                                }
                                if (!IsSelected(*g_app, unit->unit_id)) {
                                    g_app->selected_units.push_back(unit->unit_id);
                                }
                            }
                        } else if (!additive) {
                            g_app->selected_units.clear();
                        }
                        SyncAutomationEditor(*g_app, false);
                    } else if (g_app->command_mode == CommandMode::Attack) {
                        if (auto unit = FindUnitAtPoint(*g_app, board, point); unit.has_value() && !IsFriendlyUnit(*g_app, *unit)) {
                            IssueAttackCommand(*g_app, dbd::AttackTargetKind::Unit, unit->unit_id);
                        } else if (auto site = FindConstructionAtPoint(*g_app, board, point); site.has_value()) {
                            IssueAttackCommand(*g_app, dbd::AttackTargetKind::ConstructionSite, site->construction_site_id);
                        } else if (auto structure = FindStructureAtPoint(*g_app, board, point); structure.has_value()) {
                            IssueAttackCommand(*g_app, dbd::AttackTargetKind::Structure, structure->structure_id);
                        }
                    } else if (g_app->command_mode == CommandMode::Intercept) {
                        if (auto unit = FindUnitAtPoint(*g_app, board, point); unit.has_value() && !IsFriendlyUnit(*g_app, *unit)) {
                            IssueInterceptCommand(*g_app, unit->unit_id);
                        }
                    } else if (g_app->command_mode == CommandMode::Harvest) {
                        if (!g_app->selected_units.empty()) {
                            if (auto node = FindNodeAtPoint(*g_app, board, point); node.has_value()) {
                                IssueHarvestCommand(*g_app, g_app->selected_units, node->resource_node_id);
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::Loot) {
                        if (!g_app->selected_units.empty()) {
                            if (auto drop = FindDroppedCargoAtPoint(*g_app, board, point); drop.has_value()) {
                                IssueLootCommand(*g_app, drop->dropped_cargo_id);
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::HaulRoute) {
                        if (!g_app->selected_units.empty()) {
                            if (!g_app->pending_haul_source_set) {
                                if (auto node = FindNodeAtPoint(*g_app, board, point); node.has_value()) {
                                    g_app->pending_haul_source_kind = "ResourceNode";
                                    g_app->pending_haul_source_id = node->resource_node_id;
                                    g_app->pending_haul_source_set = true;
                                } else if (auto drop = FindDroppedCargoAtPoint(*g_app, board, point); drop.has_value()) {
                                    g_app->pending_haul_source_kind = "DroppedCargo";
                                    g_app->pending_haul_source_id = drop->dropped_cargo_id;
                                    g_app->pending_haul_source_set = true;
                                }
                            } else {
                                Id storage_id = 0;
                                if (auto storage = FindStorageAtPoint(*g_app, board, point); storage.has_value()) {
                                    storage_id = storage->storage_site_id;
                                }
                                IssueHaulRouteCommand(*g_app, g_app->pending_haul_source_kind, g_app->pending_haul_source_id, storage_id);
                                g_app->pending_haul_source_set = false;
                                g_app->pending_haul_source_kind = "None";
                                g_app->pending_haul_source_id = 0;
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::Guard) {
                        if (!g_app->selected_units.empty()) {
                            if (auto unit = FindUnitAtPoint(*g_app, board, point); unit.has_value() && IsFriendlyUnit(*g_app, *unit)) {
                                IssueGuardUnitCommand(*g_app, unit->unit_id);
                            } else if (auto site = FindConstructionAtPoint(*g_app, board, point); site.has_value()) {
                                IssueGuardSiteCommand(*g_app, dbd::AttackTargetKind::ConstructionSite, site->construction_site_id);
                            } else if (auto structure = FindStructureAtPoint(*g_app, board, point); structure.has_value()) {
                                IssueGuardSiteCommand(*g_app, dbd::AttackTargetKind::Structure, structure->structure_id);
                            } else {
                                IssueHoldPositionCommand(*g_app, ScreenToWorld(*g_app, point, board));
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::Repair) {
                        if (!g_app->selected_units.empty()) {
                            if (auto site = FindConstructionAtPoint(*g_app, board, point); site.has_value()) {
                                IssueRepairConstructionSiteCommand(*g_app, site->construction_site_id);
                            } else if (auto structure = FindStructureAtPoint(*g_app, board, point); structure.has_value()) {
                                IssueRepairStructureCommand(*g_app, structure->structure_id);
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::RepairSupply) {
                        if (!g_app->selected_units.empty()) {
                            if (auto site = FindConstructionAtPoint(*g_app, board, point); site.has_value()) {
                                IssueSupplyRepairCommand(*g_app, dbd::AttackTargetKind::ConstructionSite, site->construction_site_id);
                            } else if (auto structure = FindStructureAtPoint(*g_app, board, point); structure.has_value()) {
                                IssueSupplyRepairCommand(*g_app, dbd::AttackTargetKind::Structure, structure->structure_id);
                            }
                        }
                    } else if (g_app->command_mode == CommandMode::ScoutArea) {
                        if (g_app->queue_next_order) {
                            IssueQueueScoutAreaCommand(*g_app, ScreenToWorld(*g_app, point, board));
                            g_app->queue_next_order = false;
                        } else {
                            IssueScoutAreaCommand(*g_app, ScreenToWorld(*g_app, point, board));
                        }
                    } else if (g_app->command_mode == CommandMode::PatrolRoute) {
                        const auto world_point = ScreenToWorld(*g_app, point, board);
                        if (!g_app->pending_patrol_start_set) {
                            g_app->pending_patrol_start = world_point;
                            g_app->pending_patrol_start_set = true;
                        } else {
                            if (g_app->queue_next_order) {
                                IssueQueuePatrolRouteCommand(*g_app, g_app->pending_patrol_start, world_point);
                            } else {
                                IssuePatrolRouteCommand(*g_app, g_app->pending_patrol_start, world_point);
                            }
                            g_app->pending_patrol_start_set = false;
                            g_app->queue_next_order = false;
                        }
                    } else if (g_app->command_mode == CommandMode::InvestigateContact) {
                        IssueInvestigateContactCommand(*g_app);
                    } else if (g_app->command_mode == CommandMode::Flatten) {
                        IssueFlattenCommand(*g_app, ScreenToWorld(*g_app, point, board));
                    } else if (g_app->command_mode == CommandMode::Build) {
                        IssueConstructionCommand(*g_app, ScreenToWorld(*g_app, point, board));
                    }
                } else {
                    SelectUnitsInRect(*g_app, board, additive);
                    SyncAutomationEditor(*g_app, false);
                }

                g_app->dragging_selection = false;
                ReleaseCapture();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_RBUTTONUP:
            if (g_app != nullptr) {
                RECT board = GetBoardRect(hwnd);
                const POINT point {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                if (PtInRect(&board, point)) {
                    const bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (!shift_down) {
                        const auto node = FindNodeAtPoint(*g_app, board, point);
                        if (node.has_value() && IsDepotRunObjectiveNode(*g_app, *node) && !g_app->selected_units.empty()) {
                            IssueHarvestCommand(*g_app, g_app->selected_units, node->resource_node_id);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        }
                    }
                    if (shift_down) {
                        IssueQueueMoveCommand(*g_app, ScreenToWorld(*g_app, point, board));
                    } else {
                        IssueFormationMoveCommand(*g_app, ScreenToWorld(*g_app, point, board));
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        case WM_KEYDOWN:
            if (g_app != nullptr) {
                const bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool is_repeat = (l_param & (1u << 30)) != 0;
                if (w_param == VK_F5) {
                    CycleAutomationScope(*g_app);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (w_param == VK_F6) {
                    CycleAutomationSquad(*g_app, shift_down ? -1 : 1);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (w_param == VK_INSERT) {
                    g_app->automation_editor.working_rules.push_back({});
                    g_app->automation_editor.selected_rule_index = g_app->automation_editor.working_rules.size() - 1;
                    MarkAutomationDirty(*g_app);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (w_param == VK_DELETE) {
                    if (!g_app->automation_editor.working_rules.empty()) {
                        g_app->automation_editor.working_rules.erase(
                            g_app->automation_editor.working_rules.begin() + static_cast<std::ptrdiff_t>(g_app->automation_editor.selected_rule_index));
                        if (!g_app->automation_editor.working_rules.empty()) {
                            g_app->automation_editor.selected_rule_index = std::min(
                                g_app->automation_editor.selected_rule_index,
                                g_app->automation_editor.working_rules.size() - 1);
                        } else {
                            g_app->automation_editor.selected_rule_index = 0;
                        }
                        MarkAutomationDirty(*g_app);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }
                if (w_param == VK_RETURN) {
                    IssueAutomationCommand(*g_app);
                    g_app->automation_editor.dirty = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (w_param == VK_UP) {
                    if (g_app->automation_editor.selected_rule_index > 0) {
                        --g_app->automation_editor.selected_rule_index;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }
                if (w_param == VK_DOWN) {
                    if (g_app->automation_editor.selected_rule_index + 1 < g_app->automation_editor.working_rules.size()) {
                        ++g_app->automation_editor.selected_rule_index;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }
                if (w_param == 'X' && shift_down) {
                    IssueClearQueueCommand(*g_app);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (dbd::AutomationRule* rule = SelectedAutomationRule(*g_app); rule != nullptr) {
                    switch (w_param) {
                        case VK_LEFT:
                            rule->threshold = std::max(0.0f, rule->threshold - (shift_down ? 0.10f : 0.05f));
                            MarkAutomationDirty(*g_app);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        case VK_RIGHT:
                            rule->threshold = std::min(1.0f, rule->threshold + (shift_down ? 0.10f : 0.05f));
                            MarkAutomationDirty(*g_app);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        case 'Z':
                            if (!ctrl_down) {
                                break;
                            }
                            rule->trigger = NextAutomationTrigger(rule->trigger);
                            MarkAutomationDirty(*g_app);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        case 'X':
                            rule->action = NextAutomationAction(rule->action);
                            MarkAutomationDirty(*g_app);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        case 'C':
                            rule->enabled = !rule->enabled;
                            MarkAutomationDirty(*g_app);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        default:
                            break;
                    }
                }
                if (w_param >= '1' && w_param <= '4') {
                    const int slot = static_cast<int>(w_param - '1');
                    if (ctrl_down) {
                        StoreSelectionToGroup(*g_app, slot);
                    } else {
                        RecallGroupHotkey(*g_app, slot, shift_down, is_repeat);
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                switch (w_param) {
                    case VK_F1:
                        ApplyAutomationPreset(*g_app, AutomationPreset::Gather);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    case VK_F2:
                        ApplyAutomationPreset(*g_app, AutomationPreset::Escort);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    case VK_F3:
                        ApplyAutomationPreset(*g_app, AutomationPreset::Scout);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    case VK_F4:
                        ApplyAutomationPreset(*g_app, AutomationPreset::Hold);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    default:
                        break;
                }
                switch (w_param) {
                    case 'M': g_app->command_mode = CommandMode::Move; g_app->queue_next_order = false; break;
                    case 'A': g_app->command_mode = CommandMode::Attack; g_app->queue_next_order = false; break;
                    case 'I': g_app->command_mode = CommandMode::Intercept; g_app->queue_next_order = false; break;
                    case 'C': g_app->command_mode = CommandMode::Harvest; g_app->queue_next_order = false; break;
                    case 'H':
                        g_app->command_mode = CommandMode::HaulRoute;
                        g_app->queue_next_order = false;
                        g_app->pending_haul_source_set = false;
                        g_app->pending_haul_source_kind = "None";
                        g_app->pending_haul_source_id = 0;
                        break;
                    case 'L': g_app->command_mode = CommandMode::Loot; g_app->queue_next_order = false; break;
                    case 'G': g_app->command_mode = CommandMode::Guard; g_app->queue_next_order = false; break;
                    case 'Z':
                        g_app->command_mode = shift_down ? CommandMode::RepairSupply : CommandMode::Repair;
                        g_app->queue_next_order = false;
                        break;
                    case 'O':
                        g_app->command_mode = CommandMode::ScoutArea;
                        g_app->queue_next_order = shift_down;
                        g_app->pending_patrol_start_set = false;
                        break;
                    case 'P':
                        g_app->command_mode = CommandMode::PatrolRoute;
                        g_app->queue_next_order = shift_down;
                        g_app->pending_patrol_start_set = false;
                        break;
                    case 'N':
                        CycleSelectedContact(*g_app, shift_down ? -1 : 1);
                        break;
                    case 'J':
                        g_app->command_mode = CommandMode::InvestigateContact;
                        IssueInvestigateContactCommand(*g_app);
                        break;
                    case 'F': g_app->command_mode = CommandMode::Flatten; g_app->queue_next_order = false; break;
                    case 'B': g_app->command_mode = CommandMode::Build; g_app->queue_next_order = false; break;
                    case 'R': IssueReturnCommand(*g_app); break;
                    case 'Y': IssueEmergencyDepositCommand(*g_app); break;
                    case 'U': IssueDropCargoCommand(*g_app); break;
                    case 'K':
                        if (shift_down) {
                            IssueInterceptRouteThreatCommand(*g_app);
                        } else {
                            IssueGuardThreatenedRouteCommand(*g_app);
                        }
                        break;
                    case 'E': IssueRetreatCommand(*g_app); break;
                    case 'V':
                        if (!g_app->selected_units.empty()) {
                            if (const UnitView* unit = FindUnitById(*g_app, g_app->selected_units.front()); unit != nullptr) {
                                IssueHoldPositionCommand(*g_app, unit->position);
                            }
                        }
                        break;
                    case 'X': IssueStopCommand(*g_app); break;
                    case 'T': g_app->pending_structure_type = NextStructureType(g_app->pending_structure_type); break;
                    case VK_OEM_PLUS:
                    case VK_ADD: g_app->pending_flatten_radius = std::min(24.0f, g_app->pending_flatten_radius + 1.0f); break;
                    case VK_OEM_MINUS:
                    case VK_SUBTRACT: g_app->pending_flatten_radius = std::max(3.0f, g_app->pending_flatten_radius - 1.0f); break;
                    case 'W': g_app->camera.center_z -= 4.0f; break;
                    case 'S': g_app->camera.center_z += 4.0f; break;
                    case 'Q': g_app->camera.center_x -= 4.0f; break;
                    case 'D': g_app->camera.center_x += 4.0f; break;
                    case VK_ESCAPE:
                        g_app->selected_units.clear();
                        g_app->pending_patrol_start_set = false;
                        g_app->pending_haul_source_set = false;
                        g_app->pending_haul_source_kind = "None";
                        g_app->pending_haul_source_id = 0;
                        g_app->queue_next_order = false;
                        g_app->command_mode = CommandMode::Move;
                        break;
                    default:
                        break;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintApp(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmd_show) {
    AppState app;
    app.root_dir = std::filesystem::path(DBD_REBOOT_ROOT);
    app.runtime_dir = app.root_dir / "runtime-save";
    std::filesystem::create_directories(app.runtime_dir);
    g_app = &app;
    RefreshSnapshot(app);

    WNDCLASSW wc {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"DBD Reboot Native Client",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1440,
        900,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, cmd_show);

    MSG msg {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
