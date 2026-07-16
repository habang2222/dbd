#include "dbd/domain_types.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

using Id = dbd::Id;
namespace fs = std::filesystem;
using namespace DirectX;

struct UnitView {
    Id unit_id {};
    Id owner_player_id {};
    Id controller_player_id {};
    dbd::Vec3 position {};
    dbd::Vec3 move_target {};
    std::string order {"Idle"};
    std::string combat_band {"Mid"};
    float health {};
    float max_health {100.0f};
    float stamina {};
    float cargo_weight {};
    float carry_capacity {1.0f};
    Id cargo_primary_item_id {};
    std::string cargo_primary_item_name {"Empty"};
    bool alive {true};
    bool route_active {};
    std::string route_threat_level {"safe"};
    Id route_threat_enemy_id {};
    std::string route_phase {};
    std::string tactical_state {};
    Id assignment_target_entity_id {};
    dbd::Vec3 assignment_target_position {};
    std::size_t queued_order_count {};
};

struct ResourceNodeView {
    Id resource_node_id {};
    dbd::Vec3 position {};
    std::string risk_band {"Low"};
    Id produces_item_id {};
    std::string produces_item_name {"Unknown resource"};
};

struct StructureView {
    Id structure_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    float health {};
    float max_health {1.0f};
    std::string structure_type {"StorageDepot"};
};

struct StorageSiteView {
    Id storage_site_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    std::size_t stored_stack_count {};
    std::uint32_t basic_wood_amount {};
    std::uint32_t stone_block_amount {};
    std::uint32_t iron_fitting_amount {};
    std::uint32_t repair_material_amount {};
    std::uint32_t field_shovel_amount {};
    std::uint32_t survey_marker_amount {};
    std::uint32_t storage_crate_amount {};
    std::uint32_t field_hammer_amount {};
    std::string primary_item_name {"Empty"};
};

struct ConstructionSiteView {
    Id construction_site_id {};
    Id owner_player_id {};
    dbd::Vec3 position {};
    float footprint_radius {6.0f};
    float health {};
    float max_health {1.0f};
    float completion_ratio {};
    std::string structure_type {"StorageDepot"};
};

struct FlattenJobView {
    Id flatten_job_id {};
    Id owner_player_id {};
    dbd::Vec3 center {};
    float radius {6.0f};
    float required_labor {1.0f};
    float accumulated_labor {};
    std::size_t assigned_count {};
};

struct DroppedCargoView {
    Id dropped_cargo_id {};
    dbd::Vec3 position {};
    std::string primary_item_name {"Unknown cargo"};
    std::size_t stack_count {};
    float total_weight {};
};

struct KnownContactView {
    Id target_entity_id {};
    std::string target_kind {"Unit"};
    dbd::Vec3 position {};
    Id spotted_by_unit_id {};
    std::string freshness {"stale"};
};

struct ChunkView {
    int x {};
    int z {};
    bool loaded {};
    dbd::Vec3 min {};
    dbd::Vec3 max {};
};

struct TerrainTileView {
    dbd::Vec3 center {};
    float size {10.0f};
    float height {};
    std::string terrain_kind {"Ground"};
    bool flattened {};
};

struct SnapshotView {
    float world_time_seconds {};
    float day_length_seconds {1200.0f};
    float day_fraction {};
    float night_factor {};
    std::vector<UnitView> units {};
    std::vector<ResourceNodeView> resource_nodes {};
    std::vector<StructureView> structures {};
    std::vector<StorageSiteView> storage_sites {};
    std::vector<FlattenJobView> flatten_jobs {};
    std::vector<ConstructionSiteView> construction_sites {};
    std::vector<DroppedCargoView> dropped_cargo {};
    std::vector<KnownContactView> known_contacts {};
    std::vector<ChunkView> chunks {};
    std::vector<TerrainTileView> terrain_tiles {};
};

struct RenderUnitPosition {
    Id unit_id {};
    dbd::Vec3 position {};
};

struct ScenarioStatus {
    bool present {};
    std::string name {"Depot Run v0"};
    std::string state {"active"};
    std::string reason {"in_progress"};
    std::string summary {};
    float iron_fitting_stored {};
    float iron_fitting_goal {15.0f};
    Id iron_node_id {};
    std::uint64_t remaining_ticks {};
};

struct SessionStatus {
    std::uint64_t tick {};
    std::string mode {"unknown"};
    std::string last_command_message {};
    std::string runtime_alert {};
    std::string combat_alert {};
    std::string contact_alert {};
    std::string supply_alert {};
    std::string repair_alert {};
    ScenarioStatus scenario {};
};

enum class CommandMode {
    Move,
    Attack,
    Intercept,
    Harvest,
    Loot,
    HaulRoute,
    Guard,
    Repair,
    RepairSupply,
    ScoutArea,
    PatrolRoute,
    Flatten,
    Build,
    Install
};

struct Vertex {
    XMFLOAT3 position {};
    XMFLOAT4 color {};
};

struct TerrainChunkMeshCache {
    int chunk_x {};
    int chunk_z {};
    std::uint64_t content_signature {};
    int lighting_bucket {-1};
    std::vector<Vertex> vertices {};
};

struct TerrainHeightLookup {
    float bin_size {10.0f};
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> tile_bins {};
};

struct ConstantBuffer {
    XMMATRIX mvp {};
};

struct Camera {
    float center_x {50.0f};
    float center_z {30.0f};
    float distance {95.0f};
};

struct RuntimeFileCache {
    bool has_metadata {};
    fs::file_time_type write_time {};
    std::uintmax_t size {};
    std::uint64_t content_hash {};
};

struct AppState {
    HWND hwnd {};
    fs::path repo_root {};
    fs::path runtime_dir {};
    SnapshotView snapshot {};
    SessionStatus session {};
    std::vector<RenderUnitPosition> render_unit_positions {};
    std::chrono::steady_clock::time_point last_poll {};
    RuntimeFileCache snapshot_file_cache {};
    RuntimeFileCache session_file_cache {};
    std::vector<Vertex> frame_triangles {};
    std::vector<Vertex> frame_lines {};
    std::vector<Id> selected_units {};
    Id primary_player_id {};
    Camera camera {};
    CommandMode command_mode {CommandMode::Move};
    bool queue_next_order {};
    bool pending_patrol_start_set {};
    bool pending_haul_source_set {};
    dbd::Vec3 pending_patrol_start {};
    std::string pending_haul_source_kind {};
    Id pending_haul_source_id {};
    float pending_flatten_radius {8.0f};
    Id selected_contact_id {};
    std::string selected_contact_kind {"Unit"};
    bool camera_focused_on_units {};
    bool camera_moved_by_user {};
    bool inventory_open {};
    POINT mouse {};
    bool mouse_valid {};
    std::uint64_t command_sequence {};
    std::unordered_map<std::uint64_t, TerrainChunkMeshCache> terrain_chunk_mesh_cache {};
    TerrainHeightLookup terrain_height_lookup {};
    std::unordered_map<std::uint64_t, float> frame_ground_height_cache {};
    std::wstring status_line {L"Waiting for dbd_play_session snapshot..."};
};

struct D3DState {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap_chain;
    ComPtr<ID3D11RenderTargetView> render_target;
    ComPtr<ID3D11DepthStencilView> depth_view;
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11InputLayout> input_layout;
    ComPtr<ID3D11Buffer> triangle_buffer;
    ComPtr<ID3D11Buffer> line_buffer;
    ComPtr<ID3D11Buffer> constant_buffer;
    UINT width {1280};
    UINT height {760};
};

AppState g_app;
D3DState g_d3d;

constexpr float kPollSeconds = 0.10f;
constexpr float kRenderPositionFollowRate = 10.0f;
constexpr Id kIronFittingItemId = 91'003;

bool CanCraftStorageCrate(const StorageSiteView& storage) {
    return storage.basic_wood_amount >= 10 && storage.iron_fitting_amount >= 2;
}

bool CanCraftFieldShovel(const StorageSiteView& storage) {
    return storage.basic_wood_amount >= 4 && storage.stone_block_amount >= 1;
}

std::wstring ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 1) {
        return L"";
    }
    std::wstring out(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
    return out;
}

std::string JsonEscape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 8);
    for (const char ch : raw) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::optional<std::string> ReadTextFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::uint64_t HashText(const std::string& text) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash = kOffsetBasis;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= kPrime;
    }
    return hash;
}

std::optional<std::string> ReadChangedTextFile(const fs::path& path, RuntimeFileCache& cache) {
    std::error_code error;
    const auto write_time = fs::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    const auto size = fs::file_size(path, error);
    if (error) {
        return std::nullopt;
    }
    if (cache.has_metadata && cache.write_time == write_time && cache.size == size) {
        return std::nullopt;
    }

    const auto text = ReadTextFile(path);
    if (!text.has_value()) {
        return std::nullopt;
    }
    const auto hash = HashText(*text);
    const bool content_changed = !cache.has_metadata || cache.content_hash != hash;
    cache.has_metadata = true;
    cache.write_time = write_time;
    cache.size = size;
    cache.content_hash = hash;
    if (!content_changed) {
        return std::nullopt;
    }
    return text;
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

std::optional<Id> ExtractId(const std::string& object_text, const std::string& key) {
    const auto value = ExtractNumber(object_text, key);
    if (!value.has_value() || *value < 0.0) {
        return std::nullopt;
    }
    return static_cast<Id>(*value);
}

std::optional<std::string> ExtractQuotedString(const std::string& object_text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const auto key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = object_text.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const auto quote = object_text.find('"', colon + 1);
    if (quote == std::string::npos) {
        return std::nullopt;
    }
    std::string value;
    bool escaping = false;
    for (std::size_t i = quote + 1; i < object_text.size(); ++i) {
        const char ch = object_text[i];
        if (escaping) {
            switch (ch) {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                default: value += ch; break;
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
        value += ch;
    }
    return std::nullopt;
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
    value.x = static_cast<float>(ExtractNumber(inner, "x").value_or(0.0));
    value.y = static_cast<float>(ExtractNumber(inner, "y").value_or(0.0));
    value.z = static_cast<float>(ExtractNumber(inner, "z").value_or(0.0));
    return value;
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

SnapshotView ParseSnapshot(const std::string& raw) {
    SnapshotView snapshot;
    snapshot.world_time_seconds = static_cast<float>(ExtractNumber(raw, "worldTimeSeconds").value_or(0.0));
    snapshot.day_length_seconds = static_cast<float>(ExtractNumber(raw, "dayLengthSeconds").value_or(1200.0));
    snapshot.day_fraction = static_cast<float>(ExtractNumber(raw, "dayFraction").value_or(0.0));
    snapshot.night_factor = static_cast<float>(ExtractNumber(raw, "nightFactor").value_or(0.0));
    for (const auto& object_text : SplitArrayObjects(raw, "chunks")) {
        ChunkView chunk;
        chunk.x = static_cast<int>(ExtractNumber(object_text, "x").value_or(0.0));
        chunk.z = static_cast<int>(ExtractNumber(object_text, "z").value_or(0.0));
        chunk.loaded = ExtractBool(object_text, "loaded").value_or(false);
        chunk.min = ExtractVec3(object_text, "min").value_or(dbd::Vec3 {});
        chunk.max = ExtractVec3(object_text, "max").value_or(dbd::Vec3 {});
        snapshot.chunks.push_back(chunk);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "terrainTiles")) {
        TerrainTileView tile;
        tile.center = ExtractVec3(object_text, "center").value_or(dbd::Vec3 {});
        tile.size = static_cast<float>(ExtractNumber(object_text, "size").value_or(10.0));
        tile.height = static_cast<float>(ExtractNumber(object_text, "height").value_or(0.0));
        tile.terrain_kind = ExtractQuotedString(object_text, "terrainKind").value_or("Ground");
        tile.flattened = ExtractBool(object_text, "flattened").value_or(false);
        snapshot.terrain_tiles.push_back(tile);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "units")) {
        UnitView unit;
        unit.unit_id = ExtractId(object_text, "unitId").value_or(0);
        unit.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        unit.controller_player_id = ExtractId(object_text, "controllerPlayerId").value_or(unit.owner_player_id);
        unit.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        unit.move_target = ExtractVec3(object_text, "moveTarget").value_or(unit.position);
        unit.order = ExtractQuotedString(object_text, "order").value_or("Idle");
        unit.combat_band = ExtractQuotedString(object_text, "combatBand").value_or("Mid");
        unit.health = static_cast<float>(ExtractNumber(object_text, "health").value_or(0.0));
        unit.max_health = static_cast<float>(ExtractNumber(object_text, "maxHealth").value_or(100.0));
        unit.stamina = static_cast<float>(ExtractNumber(object_text, "stamina").value_or(0.0));
        unit.cargo_weight = static_cast<float>(ExtractNumber(object_text, "cargoWeight").value_or(0.0));
        unit.carry_capacity = static_cast<float>(ExtractNumber(object_text, "carryCapacity").value_or(1.0));
        unit.cargo_primary_item_id = ExtractId(object_text, "cargoPrimaryItemId").value_or(0);
        unit.cargo_primary_item_name = ExtractQuotedString(object_text, "cargoPrimaryItemName").value_or("Empty");
        unit.alive = ExtractBool(object_text, "alive").value_or(true);
        unit.route_active = ExtractBool(object_text, "routeActive").value_or(false);
        unit.route_threat_level = ExtractQuotedString(object_text, "routeThreatLevel").value_or("safe");
        unit.route_threat_enemy_id = ExtractId(object_text, "routeThreatEnemyId").value_or(0);
        unit.route_phase = ExtractQuotedString(object_text, "routePhase").value_or("");
        unit.tactical_state = ExtractQuotedString(object_text, "tacticalState").value_or("");
        unit.assignment_target_entity_id = ExtractId(object_text, "assignmentTargetEntityId").value_or(0);
        unit.assignment_target_position = ExtractVec3(object_text, "assignmentTargetPosition").value_or(unit.move_target);
        unit.queued_order_count = static_cast<std::size_t>(ExtractNumber(object_text, "queuedOrderCount").value_or(0.0));
        snapshot.units.push_back(unit);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "resourceNodes")) {
        ResourceNodeView node;
        node.resource_node_id = ExtractId(object_text, "resourceNodeId").value_or(0);
        node.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        node.risk_band = ExtractQuotedString(object_text, "riskBand").value_or("Low");
        node.produces_item_id = ExtractId(object_text, "producesItemId").value_or(0);
        node.produces_item_name = ExtractQuotedString(object_text, "producesItemName").value_or("Unknown resource");
        snapshot.resource_nodes.push_back(node);
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
        storage.basic_wood_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "basicWoodAmount").value_or(0.0));
        storage.stone_block_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "stoneBlockAmount").value_or(0.0));
        storage.iron_fitting_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "ironFittingAmount").value_or(0.0));
        storage.repair_material_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "repairMaterialAmount").value_or(0.0));
        storage.field_shovel_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "fieldShovelAmount").value_or(0.0));
        storage.survey_marker_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "surveyMarkerAmount").value_or(0.0));
        storage.storage_crate_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "storageCrateAmount").value_or(0.0));
        storage.field_hammer_amount = static_cast<std::uint32_t>(ExtractNumber(object_text, "fieldHammerAmount").value_or(0.0));
        storage.primary_item_name = ExtractQuotedString(object_text, "primaryItemName").value_or("Empty");
        snapshot.storage_sites.push_back(storage);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "flattenJobs")) {
        FlattenJobView job;
        job.flatten_job_id = ExtractId(object_text, "flattenJobId").value_or(0);
        job.owner_player_id = ExtractId(object_text, "ownerPlayerId").value_or(0);
        job.center = ExtractVec3(object_text, "center").value_or(dbd::Vec3 {});
        job.radius = static_cast<float>(ExtractNumber(object_text, "radius").value_or(6.0));
        job.required_labor = static_cast<float>(ExtractNumber(object_text, "requiredLabor").value_or(1.0));
        job.accumulated_labor = static_cast<float>(ExtractNumber(object_text, "accumulatedLabor").value_or(0.0));
        job.assigned_count = static_cast<std::size_t>(ExtractNumber(object_text, "assignedCount").value_or(0.0));
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
        site.structure_type = ExtractQuotedString(object_text, "structureType").value_or("StorageDepot");
        snapshot.construction_sites.push_back(site);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "droppedCargo")) {
        DroppedCargoView drop;
        drop.dropped_cargo_id = ExtractId(object_text, "droppedCargoId").value_or(0);
        drop.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        drop.primary_item_name = ExtractQuotedString(object_text, "primaryItemName").value_or("Unknown cargo");
        drop.stack_count = static_cast<std::size_t>(ExtractNumber(object_text, "stackCount").value_or(0.0));
        drop.total_weight = static_cast<float>(ExtractNumber(object_text, "totalWeight").value_or(0.0));
        snapshot.dropped_cargo.push_back(drop);
    }
    for (const auto& object_text : SplitArrayObjects(raw, "knownContacts")) {
        KnownContactView contact;
        contact.target_entity_id = ExtractId(object_text, "targetEntityId").value_or(0);
        contact.target_kind = ExtractQuotedString(object_text, "targetKind").value_or("Unit");
        contact.position = ExtractVec3(object_text, "position").value_or(dbd::Vec3 {});
        contact.spotted_by_unit_id = ExtractId(object_text, "spottedByUnitId").value_or(0);
        contact.freshness = ExtractQuotedString(object_text, "freshness").value_or("stale");
        snapshot.known_contacts.push_back(contact);
    }
    return snapshot;
}

SessionStatus ParseSessionStatus(const std::string& raw) {
    SessionStatus session;
    session.tick = static_cast<std::uint64_t>(ExtractNumber(raw, "tick").value_or(0.0));
    session.mode = ExtractQuotedString(raw, "mode").value_or("unknown");
    session.last_command_message = ExtractQuotedString(raw, "lastCommandMessage").value_or("");
    session.runtime_alert = ExtractQuotedString(raw, "runtimeAlert").value_or("");
    session.combat_alert = ExtractQuotedString(raw, "combatAlert").value_or("");
    session.contact_alert = ExtractQuotedString(raw, "contactAlert").value_or("");
    session.supply_alert = ExtractQuotedString(raw, "supplyAlert").value_or("");
    session.repair_alert = ExtractQuotedString(raw, "repairAlert").value_or("");
    if (const auto scenario_text = ExtractObjectText(raw, "scenario"); scenario_text.has_value()) {
        session.scenario.present = true;
        session.scenario.name = ExtractQuotedString(*scenario_text, "name").value_or("Depot Run v0");
        session.scenario.state = ExtractQuotedString(*scenario_text, "state").value_or("active");
        session.scenario.reason = ExtractQuotedString(*scenario_text, "reason").value_or("in_progress");
        session.scenario.summary = ExtractQuotedString(*scenario_text, "summary").value_or("");
        session.scenario.iron_fitting_stored = static_cast<float>(ExtractNumber(*scenario_text, "ironFittingStored").value_or(0.0));
        session.scenario.iron_fitting_goal = static_cast<float>(ExtractNumber(*scenario_text, "ironFittingGoal").value_or(15.0));
        session.scenario.iron_node_id = ExtractId(*scenario_text, "ironNodeId").value_or(0);
        session.scenario.remaining_ticks = static_cast<std::uint64_t>(ExtractNumber(*scenario_text, "remainingTicks").value_or(0.0));
    }
    return session;
}

XMFLOAT4 Color(float r, float g, float b, float a = 1.0f) {
    return {r, g, b, a};
}

XMFLOAT4 Tint(XMFLOAT4 color, float factor) {
    return {
        std::clamp(color.x * factor, 0.0f, 1.0f),
        std::clamp(color.y * factor, 0.0f, 1.0f),
        std::clamp(color.z * factor, 0.0f, 1.0f),
        color.w
    };
}

float PaletteFactor(float world_x, float world_z) {
    const int x = static_cast<int>(std::floor(world_x * 0.18f));
    const int z = static_cast<int>(std::floor(world_z * 0.18f));
    const int wrapped = ((x * 3 + z * 5) % 10 + 10) % 10;
    return 0.86f + static_cast<float>(wrapped) * 0.028f;
}

XMFLOAT4 PaletteTint(XMFLOAT4 color, float world_x, float world_z) {
    return Tint(color, PaletteFactor(world_x, world_z));
}

float ClientRiverCenterZ(float x) {
    return 72.0f + std::sin(x * 0.032f) * 14.0f;
}

float ClientMountainRise(float x, float z, float center_x, float center_z, float peak, float radius) {
    const float dx = x - center_x;
    const float dz = z - center_z;
    return peak * std::exp(-((dx * dx) + (dz * dz)) / std::max(1.0f, radius * radius));
}

float ClientPseudoTerrainHeight(float x, float z) {
    const float rolling = std::sin(x * 0.017f) * 3.5f + std::cos(z * 0.021f) * 2.8f;
    const float mountain =
        ClientMountainRise(x, z, 124.0f, 128.0f, 26.0f, 58.0f) +
        ClientMountainRise(x, z, 38.0f, 148.0f, 18.0f, 42.0f);
    const float river_distance = std::abs(z - ClientRiverCenterZ(x));
    const float river_cut = river_distance < 11.0f ? (11.0f - river_distance) * 0.72f : 0.0f;
    return rolling + mountain - river_cut;
}

bool ClientIsRiverTerrain(float x, float z) {
    return std::abs(z - ClientRiverCenterZ(x)) < 9.0f;
}

std::uint64_t PackTerrainKey(int x, int z) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
        static_cast<std::uint32_t>(z);
}

int TerrainLookupCell(float coordinate, float bin_size) {
    return static_cast<int>(std::floor(coordinate / std::max(0.001f, bin_size)));
}

void RebuildTerrainHeightLookup() {
    auto& lookup = g_app.terrain_height_lookup;
    lookup.tile_bins.clear();
    lookup.bin_size = 10.0f;
    if (g_app.snapshot.terrain_tiles.empty()) {
        return;
    }

    float smallest_tile = std::numeric_limits<float>::max();
    for (const auto& tile : g_app.snapshot.terrain_tiles) {
        smallest_tile = std::min(smallest_tile, std::max(0.25f, tile.size));
    }
    lookup.bin_size = std::clamp(smallest_tile, 0.25f, 64.0f);
    lookup.tile_bins.reserve(g_app.snapshot.terrain_tiles.size() * 2);

    for (std::size_t tile_index = 0; tile_index < g_app.snapshot.terrain_tiles.size(); ++tile_index) {
        const auto& tile = g_app.snapshot.terrain_tiles[tile_index];
        const float half = std::max(0.0f, tile.size * 0.5f);
        const int min_x = TerrainLookupCell(tile.center.x - half, lookup.bin_size);
        const int max_x = TerrainLookupCell(tile.center.x + half, lookup.bin_size);
        const int min_z = TerrainLookupCell(tile.center.z - half, lookup.bin_size);
        const int max_z = TerrainLookupCell(tile.center.z + half, lookup.bin_size);
        for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
            for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
                lookup.tile_bins[PackTerrainKey(cell_x, cell_z)].push_back(tile_index);
            }
        }
    }
}

const TerrainTileView* FindTerrainTileAt(float x, float z) {
    const auto& lookup = g_app.terrain_height_lookup;
    if (lookup.tile_bins.empty()) {
        return nullptr;
    }

    const int cell_x = TerrainLookupCell(x, lookup.bin_size);
    const int cell_z = TerrainLookupCell(z, lookup.bin_size);
    const auto bucket = lookup.tile_bins.find(PackTerrainKey(cell_x, cell_z));
    if (bucket == lookup.tile_bins.end()) {
        return nullptr;
    }

    for (const std::size_t tile_index : bucket->second) {
        if (tile_index >= g_app.snapshot.terrain_tiles.size()) {
            continue;
        }
        const auto& tile = g_app.snapshot.terrain_tiles[tile_index];
        const float half = tile.size * 0.5f;
        if (std::abs(tile.center.x - x) <= half && std::abs(tile.center.z - z) <= half) {
            return &tile;
        }
    }
    return nullptr;
}

std::uint64_t GroundHeightPointKey(float x, float z) {
    constexpr float kHeightCachePrecision = 64.0f;
    const int quantized_x = static_cast<int>(std::round(x * kHeightCachePrecision));
    const int quantized_z = static_cast<int>(std::round(z * kHeightCachePrecision));
    return PackTerrainKey(quantized_x, quantized_z);
}

float RenderGroundHeight(float x, float z) {
    const std::uint64_t point_key = GroundHeightPointKey(x, z);
    if (const auto cached = g_app.frame_ground_height_cache.find(point_key); cached != g_app.frame_ground_height_cache.end()) {
        return cached->second;
    }

    float height = std::max(0.08f, ClientPseudoTerrainHeight(x, z));
    if (const TerrainTileView* tile = FindTerrainTileAt(x, z); tile != nullptr && tile->flattened) {
        height = std::max(0.08f, tile->height);
    }
    g_app.frame_ground_height_cache.emplace(point_key, height);
    return height;
}

float StableFootprintGroundHeight(float x, float z, float radius) {
    const float center = RenderGroundHeight(x, z);
    const float sample_radius = std::max(0.6f, radius);
    const float samples[] {
        center,
        RenderGroundHeight(x - sample_radius, z - sample_radius),
        RenderGroundHeight(x + sample_radius, z - sample_radius),
        RenderGroundHeight(x - sample_radius, z + sample_radius),
        RenderGroundHeight(x + sample_radius, z + sample_radius),
        RenderGroundHeight(x - sample_radius, z),
        RenderGroundHeight(x + sample_radius, z),
        RenderGroundHeight(x, z - sample_radius),
        RenderGroundHeight(x, z + sample_radius)
    };
    float total = 0.0f;
    float min_height = samples[0];
    for (float sample : samples) {
        total += sample;
        min_height = std::min(min_height, sample);
    }
    const float average = total / static_cast<float>(std::size(samples));
    return std::max(min_height + 0.05f, average);
}

XMFLOAT3 V3(float x, float y, float z) {
    return {x, y, z};
}

XMFLOAT3 WorldPoint(const dbd::Vec3& value, float y = 0.0f) {
    return {value.x, RenderGroundHeight(value.x, value.z) + y, value.z};
}

float DistanceSq2D(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return (dx * dx) + (dz * dz);
}

const UnitView* FindUnitById(Id unit_id) {
    const auto it = std::find_if(g_app.snapshot.units.begin(), g_app.snapshot.units.end(), [unit_id](const UnitView& unit) {
        return unit.unit_id == unit_id;
    });
    return it == g_app.snapshot.units.end() ? nullptr : &(*it);
}

bool IsCommandableUnit(const UnitView& unit) {
    return unit.alive &&
        g_app.primary_player_id != 0 &&
        (unit.owner_player_id == g_app.primary_player_id || unit.controller_player_id == g_app.primary_player_id);
}

void PruneSelectedUnits() {
    const auto old_size = g_app.selected_units.size();
    g_app.selected_units.erase(
        std::remove_if(
            g_app.selected_units.begin(),
            g_app.selected_units.end(),
            [](Id unit_id) {
                const UnitView* unit = FindUnitById(unit_id);
                return unit == nullptr || !IsCommandableUnit(*unit);
            }),
        g_app.selected_units.end());
    if (old_size != g_app.selected_units.size()) {
        g_app.status_line = L"Selection pruned: dead or non-commandable unit removed.";
    }
}

dbd::Vec3 TargetPositionForUnit(Id unit_id, dbd::Vec3 fallback) {
    if (const auto* unit = FindUnitById(unit_id)) {
        return unit->position;
    }
    return fallback;
}

dbd::Vec3 RenderPositionForUnit(const UnitView& unit) {
    const auto it = std::find_if(g_app.render_unit_positions.begin(), g_app.render_unit_positions.end(), [&unit](const RenderUnitPosition& render_unit) {
        return render_unit.unit_id == unit.unit_id;
    });
    return it == g_app.render_unit_positions.end() ? unit.position : it->position;
}

void SyncRenderUnitPositionsAfterSnapshot() {
    for (const auto& unit : g_app.snapshot.units) {
        const auto it = std::find_if(g_app.render_unit_positions.begin(), g_app.render_unit_positions.end(), [&unit](const RenderUnitPosition& render_unit) {
            return render_unit.unit_id == unit.unit_id;
        });
        if (it == g_app.render_unit_positions.end()) {
            g_app.render_unit_positions.push_back({unit.unit_id, unit.position});
        }
    }

    g_app.render_unit_positions.erase(
        std::remove_if(
            g_app.render_unit_positions.begin(),
            g_app.render_unit_positions.end(),
            [](const RenderUnitPosition& render_unit) {
                return FindUnitById(render_unit.unit_id) == nullptr;
            }),
        g_app.render_unit_positions.end());
}

void UpdateRenderInterpolation(float delta_seconds) {
    const float alpha = std::clamp(delta_seconds * kRenderPositionFollowRate, 0.0f, 1.0f);
    for (auto& render_unit : g_app.render_unit_positions) {
        const dbd::Vec3 target = TargetPositionForUnit(render_unit.unit_id, render_unit.position);
        const float dx = target.x - render_unit.position.x;
        const float dy = target.y - render_unit.position.y;
        const float dz = target.z - render_unit.position.z;
        const float distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
        if (distance_sq > 625.0f) {
            render_unit.position = target;
            continue;
        }
        render_unit.position.x += dx * alpha;
        render_unit.position.y += dy * alpha;
        render_unit.position.z += dz * alpha;
    }
}

const ResourceNodeView* FindResourceNodeById(Id node_id) {
    const auto it = std::find_if(g_app.snapshot.resource_nodes.begin(), g_app.snapshot.resource_nodes.end(), [node_id](const ResourceNodeView& node) {
        return node.resource_node_id == node_id;
    });
    return it == g_app.snapshot.resource_nodes.end() ? nullptr : &(*it);
}

const StorageSiteView* FindStorageSiteById(Id storage_id) {
    const auto it = std::find_if(g_app.snapshot.storage_sites.begin(), g_app.snapshot.storage_sites.end(), [storage_id](const StorageSiteView& storage) {
        return storage.storage_site_id == storage_id;
    });
    return it == g_app.snapshot.storage_sites.end() ? nullptr : &(*it);
}

const StructureView* FindStructureById(Id structure_id) {
    const auto it = std::find_if(g_app.snapshot.structures.begin(), g_app.snapshot.structures.end(), [structure_id](const StructureView& structure) {
        return structure.structure_id == structure_id;
    });
    return it == g_app.snapshot.structures.end() ? nullptr : &(*it);
}

const ConstructionSiteView* FindConstructionSiteById(Id site_id) {
    const auto it = std::find_if(g_app.snapshot.construction_sites.begin(), g_app.snapshot.construction_sites.end(), [site_id](const ConstructionSiteView& site) {
        return site.construction_site_id == site_id;
    });
    return it == g_app.snapshot.construction_sites.end() ? nullptr : &(*it);
}

const DroppedCargoView* FindDroppedCargoById(Id drop_id) {
    const auto it = std::find_if(g_app.snapshot.dropped_cargo.begin(), g_app.snapshot.dropped_cargo.end(), [drop_id](const DroppedCargoView& drop) {
        return drop.dropped_cargo_id == drop_id;
    });
    return it == g_app.snapshot.dropped_cargo.end() ? nullptr : &(*it);
}

const StorageSiteView* FindNearestStorage(const dbd::Vec3& point) {
    const StorageSiteView* best = nullptr;
    float best_distance = 1.0e9f;
    for (const auto& storage : g_app.snapshot.storage_sites) {
        if (storage.owner_player_id != g_app.primary_player_id) {
            continue;
        }
        const float distance = DistanceSq2D(point, storage.position);
        if (distance < best_distance) {
            best_distance = distance;
            best = &storage;
        }
    }
    return best;
}

bool IsSelected(Id unit_id) {
    return std::find(g_app.selected_units.begin(), g_app.selected_units.end(), unit_id) != g_app.selected_units.end();
}

bool IsObjectiveCargo(const UnitView& unit) {
    return unit.cargo_primary_item_id == kIronFittingItemId || unit.cargo_primary_item_name == "Iron Fitting";
}

bool IsObjectiveNode(const ResourceNodeView& node) {
    return g_app.session.scenario.present && node.resource_node_id == g_app.session.scenario.iron_node_id;
}

std::string LowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string ResourcePlaceholder(const ResourceNodeView& node) {
    const auto name = LowerCopy(node.produces_item_name);
    if (name.find("iron") != std::string::npos) {
        return IsObjectiveNode(node) ? "OBJ iron" : "iron";
    }
    if (name.find("stone") != std::string::npos) {
        return "stone";
    }
    if (name.find("wood") != std::string::npos) {
        return "wood";
    }
    return "resource";
}

std::wstring CommandModeText() {
    switch (g_app.command_mode) {
        case CommandMode::Move: return L"Move";
        case CommandMode::Attack: return L"Focus Fire";
        case CommandMode::Intercept: return L"Intercept";
        case CommandMode::Harvest: return L"Harvest";
        case CommandMode::Loot: return L"Loot";
        case CommandMode::HaulRoute: return g_app.pending_haul_source_set ? L"Haul Route: pick depot/ground" : L"Haul Route: pick source";
        case CommandMode::Guard: return L"Guard";
        case CommandMode::Repair: return L"Repair";
        case CommandMode::RepairSupply: return L"Supply Repair";
        case CommandMode::ScoutArea: return g_app.queue_next_order ? L"Queue Scout" : L"Scout";
        case CommandMode::PatrolRoute: return g_app.pending_patrol_start_set ? L"Patrol: pick B" : (g_app.queue_next_order ? L"Queue Patrol: pick A" : L"Patrol: pick A");
        case CommandMode::Flatten: return L"Flatten";
        case CommandMode::Build: return L"Build";
        case CommandMode::Install: return L"Install Storage Crate";
    }
    return L"Move";
}

std::string ObjectiveCargoState(const UnitView& unit) {
    if (!IsObjectiveCargo(unit)) {
        return "";
    }
    if (unit.order == "AttackTarget") {
        return "IRON COMBAT";
    }
    if (unit.route_active && unit.route_threat_level == "critical") {
        return "IRON ROUTE CRITICAL";
    }
    if (unit.route_active && unit.route_threat_level == "threatened") {
        return "IRON THREAT";
    }
    for (const auto& other : g_app.snapshot.units) {
        if (!other.alive || other.owner_player_id == unit.owner_player_id) {
            continue;
        }
        if (DistanceSq2D(other.position, unit.position) <= 20.0f * 20.0f) {
            return "IRON THREAT";
        }
    }
    return "IRON";
}

XMMATRIX ViewMatrix() {
    const XMVECTOR target = XMVectorSet(g_app.camera.center_x, 0.0f, g_app.camera.center_z, 1.0f);
    const XMVECTOR eye = XMVectorSet(
        g_app.camera.center_x - g_app.camera.distance * 0.62f,
        g_app.camera.distance * 0.82f,
        g_app.camera.center_z - g_app.camera.distance * 0.62f,
        1.0f);
    return XMMatrixLookAtLH(eye, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

XMMATRIX ProjectionMatrix() {
    const float aspect = g_d3d.height == 0 ? 1.0f : static_cast<float>(g_d3d.width) / static_cast<float>(g_d3d.height);
    return XMMatrixPerspectiveFovLH(XMConvertToRadians(48.0f), aspect, 0.1f, 2000.0f);
}

std::optional<POINT> WorldToScreen(const dbd::Vec3& world, float y_offset = 4.0f) {
    if (g_d3d.width == 0 || g_d3d.height == 0) {
        return std::nullopt;
    }
    const XMMATRIX view_projection = ViewMatrix() * ProjectionMatrix();
    const XMVECTOR clip = XMVector3TransformCoord(
        XMVectorSet(world.x, RenderGroundHeight(world.x, world.z) + y_offset, world.z, 1.0f),
        view_projection);
    const float x = XMVectorGetX(clip);
    const float y = XMVectorGetY(clip);
    const float z = XMVectorGetZ(clip);
    if (z < 0.0f || z > 1.0f || x < -1.25f || x > 1.25f || y < -1.25f || y > 1.25f) {
        return std::nullopt;
    }
    POINT out {};
    out.x = static_cast<LONG>((x + 1.0f) * 0.5f * static_cast<float>(g_d3d.width));
    out.y = static_cast<LONG>((1.0f - y) * 0.5f * static_cast<float>(g_d3d.height));
    return out;
}

void AddLine(std::vector<Vertex>& lines, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT4 color) {
    lines.push_back({a, color});
    lines.push_back({b, color});
}

void AddCube(std::vector<Vertex>& tris, XMFLOAT3 center, XMFLOAT3 half, XMFLOAT4 color) {
    const float x0 = center.x - half.x;
    const float x1 = center.x + half.x;
    const float y0 = center.y - half.y;
    const float y1 = center.y + half.y;
    const float z0 = center.z - half.z;
    const float z1 = center.z + half.z;
    const XMFLOAT3 p[8] {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
    };
    constexpr int idx[] {
        0, 2, 1, 0, 3, 2, 1, 2, 6, 1, 6, 5,
        5, 6, 7, 5, 7, 4, 4, 7, 3, 4, 3, 0,
        3, 7, 6, 3, 6, 2, 4, 0, 1, 4, 1, 5
    };
    for (int i : idx) {
        tris.push_back({p[i], color});
    }
}

void AddTopQuad(std::vector<Vertex>& tris, float center_x, float center_z, float half_size, float height, XMFLOAT4 color) {
    const XMFLOAT3 a {center_x - half_size, height, center_z - half_size};
    const XMFLOAT3 b {center_x + half_size, height, center_z - half_size};
    const XMFLOAT3 c {center_x + half_size, height, center_z + half_size};
    const XMFLOAT3 d {center_x - half_size, height, center_z + half_size};
    tris.push_back({a, color});
    tris.push_back({c, color});
    tris.push_back({b, color});
    tris.push_back({a, color});
    tris.push_back({d, color});
    tris.push_back({c, color});
}

std::uint64_t TerrainChunkKey(int chunk_x, int chunk_z) {
    const auto x = static_cast<std::uint32_t>(chunk_x);
    const auto z = static_cast<std::uint32_t>(chunk_z);
    return (static_cast<std::uint64_t>(x) << 32U) | static_cast<std::uint64_t>(z);
}

std::uint64_t MixTerrainSignature(std::uint64_t seed, std::uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
}

std::uint64_t QuantizedTerrainValue(float value) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(value * 1000.0f)));
}

const ChunkView* FindLoadedTerrainChunk(float world_x, float world_z) {
    for (const auto& chunk : g_app.snapshot.chunks) {
        if (!chunk.loaded) {
            continue;
        }
        if (world_x >= chunk.min.x && world_x <= chunk.max.x &&
            world_z >= chunk.min.z && world_z <= chunk.max.z) {
            return &chunk;
        }
    }
    return nullptr;
}

std::uint64_t TerrainChunkContentSignature(const std::vector<const TerrainTileView*>& tiles) {
    std::uint64_t signature = 0xcbf29ce484222325ULL;
    signature = MixTerrainSignature(signature, static_cast<std::uint64_t>(tiles.size()));
    for (const auto* tile : tiles) {
        signature = MixTerrainSignature(signature, QuantizedTerrainValue(tile->center.x));
        signature = MixTerrainSignature(signature, QuantizedTerrainValue(tile->center.z));
        signature = MixTerrainSignature(signature, QuantizedTerrainValue(tile->size));
        signature = MixTerrainSignature(signature, QuantizedTerrainValue(tile->height));
        signature = MixTerrainSignature(signature, tile->flattened ? 1ULL : 0ULL);
        signature = MixTerrainSignature(signature, static_cast<std::uint64_t>(std::hash<std::string> {}(tile->terrain_kind)));
    }
    return signature;
}

int TerrainLightingBucket() {
    return static_cast<int>(std::round(std::clamp(g_app.snapshot.night_factor, 0.0f, 1.0f) * 24.0f));
}

void BuildTerrainChunkMesh(
    TerrainChunkMeshCache& cache,
    const std::vector<const TerrainTileView*>& tiles,
    std::uint64_t content_signature,
    int lighting_bucket) {
    cache.content_signature = content_signature;
    cache.lighting_bucket = lighting_bucket;
    cache.vertices.clear();
    cache.vertices.reserve(tiles.size() * 5U * 5U * 6U);

    const float bucket_night = static_cast<float>(lighting_bucket) / 24.0f;
    constexpr int kVisualSubdivisions = 5;
    for (const auto* tile : tiles) {
        const float visual_size = tile->size / static_cast<float>(kVisualSubdivisions);
        const float tile_min_x = tile->center.x - tile->size * 0.5f;
        const float tile_min_z = tile->center.z - tile->size * 0.5f;
        for (int sub_z = 0; sub_z < kVisualSubdivisions; ++sub_z) {
            for (int sub_x = 0; sub_x < kVisualSubdivisions; ++sub_x) {
                const float center_x = tile_min_x + (static_cast<float>(sub_x) + 0.5f) * visual_size;
                const float center_z = tile_min_z + (static_cast<float>(sub_z) + 0.5f) * visual_size;
                const float sampled_height = tile->flattened
                    ? tile->height
                    : ClientPseudoTerrainHeight(center_x, center_z);
                const float raised = std::max(0.08f, sampled_height);
                const bool river = !tile->flattened && ClientIsRiverTerrain(center_x, center_z);
                const bool mountain = !river && sampled_height >= 13.0f;

                XMFLOAT4 color = Color(0.18f, 0.42f, 0.18f);
                if (river) {
                    color = Color(0.08f, 0.34f, 0.78f);
                } else if (mountain) {
                    color = Color(0.40f, 0.38f, 0.34f);
                }
                if (tile->flattened) {
                    color = Color(0.72f, 0.66f, 0.34f);
                }
                color = PaletteTint(color, center_x, center_z);
                color = Tint(color, 0.72f + (1.0f - bucket_night) * 0.28f);
                AddTopQuad(cache.vertices, center_x, center_z, visual_size * 0.505f, raised, color);
            }
        }
    }
}

void SyncTerrainChunkMeshCache() {
    std::unordered_map<std::uint64_t, std::vector<const TerrainTileView*>> tiles_by_chunk;
    tiles_by_chunk.reserve(g_app.snapshot.chunks.size());
    for (const auto& tile : g_app.snapshot.terrain_tiles) {
        const ChunkView* chunk = FindLoadedTerrainChunk(tile.center.x, tile.center.z);
        if (chunk == nullptr) {
            continue;
        }
        tiles_by_chunk[TerrainChunkKey(chunk->x, chunk->z)].push_back(&tile);
    }

    const int lighting_bucket = TerrainLightingBucket();
    for (const auto& chunk : g_app.snapshot.chunks) {
        const std::uint64_t key = TerrainChunkKey(chunk.x, chunk.z);
        if (!chunk.loaded) {
            g_app.terrain_chunk_mesh_cache.erase(key);
            continue;
        }

        const auto tiles_it = tiles_by_chunk.find(key);
        if (tiles_it == tiles_by_chunk.end()) {
            g_app.terrain_chunk_mesh_cache.erase(key);
            continue;
        }

        const std::uint64_t signature = TerrainChunkContentSignature(tiles_it->second);
        auto [cache_it, inserted] = g_app.terrain_chunk_mesh_cache.try_emplace(key);
        TerrainChunkMeshCache& cache = cache_it->second;
        cache.chunk_x = chunk.x;
        cache.chunk_z = chunk.z;
        if (inserted || cache.content_signature != signature || cache.lighting_bucket != lighting_bucket) {
            BuildTerrainChunkMesh(cache, tiles_it->second, signature, lighting_bucket);
        }
    }

    for (auto it = g_app.terrain_chunk_mesh_cache.begin(); it != g_app.terrain_chunk_mesh_cache.end();) {
        if (tiles_by_chunk.find(it->first) == tiles_by_chunk.end()) {
            it = g_app.terrain_chunk_mesh_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void AppendCachedTerrainVertices(std::vector<Vertex>& tris) {
    for (const auto& [key, cache] : g_app.terrain_chunk_mesh_cache) {
        (void)key;
        tris.insert(tris.end(), cache.vertices.begin(), cache.vertices.end());
    }
}

void AddOctahedron(std::vector<Vertex>& tris, XMFLOAT3 center, float radius, XMFLOAT4 color) {
    const XMFLOAT3 top {center.x, center.y + radius, center.z};
    const XMFLOAT3 bottom {center.x, center.y - radius, center.z};
    const XMFLOAT3 n {center.x, center.y, center.z - radius};
    const XMFLOAT3 e {center.x + radius, center.y, center.z};
    const XMFLOAT3 s {center.x, center.y, center.z + radius};
    const XMFLOAT3 w {center.x - radius, center.y, center.z};
    const XMFLOAT3 faces[][3] {
        {top, n, e}, {top, e, s}, {top, s, w}, {top, w, n},
        {bottom, e, n}, {bottom, s, e}, {bottom, w, s}, {bottom, n, w}
    };
    for (const auto& face : faces) {
        tris.push_back({face[0], color});
        tris.push_back({face[1], color});
        tris.push_back({face[2], color});
    }
}

void AddCircle(std::vector<Vertex>& lines, XMFLOAT3 center, float radius, XMFLOAT4 color) {
    constexpr int kSegments = 48;
    for (int i = 0; i < kSegments; ++i) {
        const float a0 = XM_2PI * static_cast<float>(i) / static_cast<float>(kSegments);
        const float a1 = XM_2PI * static_cast<float>(i + 1) / static_cast<float>(kSegments);
        AddLine(
            lines,
            {center.x + std::cos(a0) * radius, center.y, center.z + std::sin(a0) * radius},
            {center.x + std::cos(a1) * radius, center.y, center.z + std::sin(a1) * radius},
            color);
    }
}

void AddBoxWire(std::vector<Vertex>& lines, XMFLOAT3 center, XMFLOAT3 half, XMFLOAT4 color) {
    const float x0 = center.x - half.x;
    const float x1 = center.x + half.x;
    const float y0 = center.y - half.y;
    const float y1 = center.y + half.y;
    const float z0 = center.z - half.z;
    const float z1 = center.z + half.z;
    const XMFLOAT3 p[8] {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
    };
    constexpr int edges[][2] {
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    for (const auto& edge : edges) {
        AddLine(lines, p[edge[0]], p[edge[1]], color);
    }
}

bool TextContains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

void AddProjectedShadow(std::vector<Vertex>& tris, const dbd::Vec3& position, float radius, float height, float night_factor) {
    const float stretch = 0.42f + night_factor * 0.42f;
    const float offset_x = -0.9f * height;
    const float offset_z = 0.55f * height;
    const float darkness = 0.04f + night_factor * 0.07f;
    const float shadow_x = position.x + offset_x;
    const float shadow_z = position.z + offset_z;
    const float ground_y = RenderGroundHeight(shadow_x, shadow_z);
    AddCube(
        tris,
        {shadow_x, ground_y + 0.035f, shadow_z},
        {radius * (1.0f + stretch), 0.035f, radius * 0.54f},
        Color(darkness, darkness + 0.01f, darkness + 0.025f));
}

void AddVoxelPerson(std::vector<Vertex>& tris, const dbd::Vec3& position, bool friendly, bool objective_cargo) {
    constexpr float s = 0.36f;
    const float ground = RenderGroundHeight(position.x, position.z);
    const XMFLOAT4 skin = friendly ? Color(0.86f, 0.67f, 0.48f) : Color(0.72f, 0.48f, 0.38f);
    const XMFLOAT4 shirt = friendly ? Color(0.16f, 0.42f, 0.92f) : Color(0.82f, 0.12f, 0.14f);
    const XMFLOAT4 pants = friendly ? Color(0.08f, 0.16f, 0.32f) : Color(0.18f, 0.10f, 0.08f);
    const XMFLOAT4 boots = Color(0.05f, 0.04f, 0.035f);

    AddCube(tris, {position.x, ground + 4.65f * s, position.z}, {0.95f * s, 0.95f * s, 0.95f * s}, PaletteTint(skin, position.x, position.z));
    AddCube(tris, {position.x, ground + 2.85f * s, position.z}, {1.05f * s, 1.05f * s, 0.72f * s}, PaletteTint(shirt, position.x, position.z));
    AddCube(tris, {position.x - 1.18f * s, ground + 2.78f * s, position.z}, {0.32f * s, 0.9f * s, 0.35f * s}, PaletteTint(shirt, position.x - 1.0f, position.z));
    AddCube(tris, {position.x + 1.18f * s, ground + 2.78f * s, position.z}, {0.32f * s, 0.9f * s, 0.35f * s}, PaletteTint(shirt, position.x + 1.0f, position.z));
    AddCube(tris, {position.x - 0.48f * s, ground + 1.18f * s, position.z}, {0.35f * s, 1.0f * s, 0.38f * s}, PaletteTint(pants, position.x, position.z - 0.5f));
    AddCube(tris, {position.x + 0.48f * s, ground + 1.18f * s, position.z}, {0.35f * s, 1.0f * s, 0.38f * s}, PaletteTint(pants, position.x, position.z + 0.5f));
    AddCube(tris, {position.x - 0.48f * s, ground + 0.2f * s, position.z + 0.08f * s}, {0.42f * s, 0.22f * s, 0.48f * s}, boots);
    AddCube(tris, {position.x + 0.48f * s, ground + 0.2f * s, position.z + 0.08f * s}, {0.42f * s, 0.22f * s, 0.48f * s}, boots);
    if (objective_cargo) {
        AddCube(tris, {position.x + 1.55f * s, ground + 2.25f * s, position.z + 0.85f * s}, {0.48f * s, 0.48f * s, 0.48f * s}, Color(0.15f, 0.75f, 1.0f));
    }
}

void AddVoxelResource(std::vector<Vertex>& tris, const ResourceNodeView& node) {
    constexpr float s = 0.72f;
    const float ground = RenderGroundHeight(node.position.x, node.position.z);
    if (node.produces_item_name == "Basic Wood") {
        AddCube(tris, {node.position.x, ground + 1.45f * s, node.position.z}, {0.75f * s, 1.45f * s, 0.75f * s}, Color(0.45f, 0.25f, 0.10f));
        AddCube(tris, {node.position.x, ground + 3.75f * s, node.position.z}, {1.9f * s, 1.25f * s, 1.9f * s}, Color(0.13f, 0.58f, 0.20f));
        AddCube(tris, {node.position.x - 1.15f * s, ground + 3.15f * s, node.position.z + 0.75f * s}, {0.95f * s, 0.85f * s, 0.95f * s}, Color(0.09f, 0.44f, 0.16f));
        return;
    }
    if (node.produces_item_name == "Iron Fitting") {
        AddCube(tris, {node.position.x, ground + 1.1f * s, node.position.z}, {1.8f * s, 1.1f * s, 1.8f * s}, Color(0.30f, 0.32f, 0.34f));
        AddCube(tris, {node.position.x - 0.75f * s, ground + 2.35f * s, node.position.z + 0.4f * s}, {0.5f * s, 0.5f * s, 0.5f * s}, Color(0.80f, 0.30f, 0.18f));
        AddCube(tris, {node.position.x + 0.55f * s, ground + 2.05f * s, node.position.z - 0.7f * s}, {0.45f * s, 0.45f * s, 0.45f * s}, Color(0.90f, 0.45f, 0.20f));
        return;
    }
    AddCube(tris, {node.position.x, ground + 0.8f * s, node.position.z}, {1.6f * s, 0.8f * s, 1.6f * s}, Color(0.55f, 0.58f, 0.60f));
    AddCube(tris, {node.position.x + 0.7f * s, ground + 1.65f * s, node.position.z - 0.45f * s}, {0.9f * s, 0.65f * s, 0.9f * s}, Color(0.42f, 0.45f, 0.47f));
    AddCube(tris, {node.position.x - 0.85f * s, ground + 1.25f * s, node.position.z + 0.65f * s}, {0.7f * s, 0.5f * s, 0.7f * s}, Color(0.63f, 0.65f, 0.66f));
}

void AddVoxelDepot(std::vector<Vertex>& tris, const dbd::Vec3& position) {
    const float ground = StableFootprintGroundHeight(position.x, position.z, 3.45f);
    AddCube(tris, {position.x, ground + 1.3f, position.z}, {3.0f, 1.3f, 3.0f}, Color(0.33f, 0.22f, 0.12f));
    AddCube(tris, {position.x, ground + 3.0f, position.z}, {3.45f, 0.55f, 3.45f}, Color(0.12f, 0.42f, 0.22f));
    AddCube(tris, {position.x - 1.15f, ground + 1.5f, position.z - 3.05f}, {0.55f, 0.9f, 0.12f}, Color(0.12f, 0.08f, 0.04f));
    AddCube(tris, {position.x + 1.15f, ground + 1.5f, position.z - 3.05f}, {0.55f, 0.9f, 0.12f}, Color(0.12f, 0.08f, 0.04f));
}

void AddVoxelStructure(std::vector<Vertex>& tris, const dbd::Vec3& position, bool damaged) {
    const float ground = StableFootprintGroundHeight(position.x, position.z, 2.8f);
    AddCube(tris, {position.x, ground + 1.35f, position.z}, {2.8f, 1.35f, 2.8f}, damaged ? Color(0.55f, 0.22f, 0.18f) : Color(0.22f, 0.48f, 0.76f));
    AddCube(tris, {position.x, ground + 3.15f, position.z}, {2.15f, 0.65f, 2.15f}, Color(0.10f, 0.18f, 0.28f));
    AddCube(tris, {position.x, ground + 4.25f, position.z}, {1.15f, 0.45f, 1.15f}, Color(0.45f, 0.60f, 0.78f));
}

void AddVoxelConstructionSite(std::vector<Vertex>& tris, const ConstructionSiteView& site) {
    const float half = std::max(1.8f, site.footprint_radius * 0.38f);
    const float ground = StableFootprintGroundHeight(site.position.x, site.position.z, half);
    AddCube(tris, {site.position.x - half, ground + 0.55f, site.position.z - half}, {0.45f, 0.55f, 0.45f}, Color(0.70f, 0.36f, 0.15f));
    AddCube(tris, {site.position.x + half, ground + 0.55f, site.position.z - half}, {0.45f, 0.55f, 0.45f}, Color(0.70f, 0.36f, 0.15f));
    AddCube(tris, {site.position.x - half, ground + 0.55f, site.position.z + half}, {0.45f, 0.55f, 0.45f}, Color(0.70f, 0.36f, 0.15f));
    AddCube(tris, {site.position.x + half, ground + 0.55f, site.position.z + half}, {0.45f, 0.55f, 0.45f}, Color(0.70f, 0.36f, 0.15f));
    AddCube(tris, {site.position.x, ground + 0.18f, site.position.z}, {half, 0.18f, half}, Color(0.47f, 0.25f, 0.12f));
}

void AddVoxelCargo(std::vector<Vertex>& tris, const DroppedCargoView& drop) {
    const bool marker = drop.primary_item_name == "Survey Marker";
    const float ground = RenderGroundHeight(drop.position.x, drop.position.z);
    if (marker) {
        AddCube(tris, {drop.position.x, ground + 1.3f, drop.position.z}, {0.18f, 1.3f, 0.18f}, Color(0.85f, 0.85f, 0.78f));
        AddCube(tris, {drop.position.x + 0.55f, ground + 2.25f, drop.position.z}, {0.55f, 0.35f, 0.08f}, Color(0.95f, 0.20f, 0.20f));
        return;
    }
    AddCube(tris, {drop.position.x, ground + 0.75f, drop.position.z}, {1.05f, 0.75f, 1.05f}, Color(0.62f, 0.37f, 0.16f));
    AddCube(tris, {drop.position.x, ground + 1.55f, drop.position.z}, {1.12f, 0.12f, 1.12f}, Color(0.22f, 0.13f, 0.06f));
    AddCube(tris, {drop.position.x, ground + 0.75f, drop.position.z}, {0.15f, 0.82f, 1.16f}, Color(0.22f, 0.13f, 0.06f));
}

HRESULT CompileShader(const char* source, const char* entry, const char* profile, ID3DBlob** blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, profile, flags, 0, blob, errors.GetAddressOf());
    if (FAILED(hr) && errors) {
        OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
    }
    return hr;
}

bool CreateDynamicBuffer(UINT byte_width, ID3D11Buffer** buffer) {
    D3D11_BUFFER_DESC desc {};
    desc.ByteWidth = byte_width;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(g_d3d.device->CreateBuffer(&desc, nullptr, buffer));
}

bool ResizeBackbuffer(UINT width, UINT height) {
    if (!g_d3d.device || !g_d3d.swap_chain) {
        return false;
    }
    g_d3d.context->OMSetRenderTargets(0, nullptr, nullptr);
    g_d3d.render_target.Reset();
    g_d3d.depth_view.Reset();

    if (FAILED(g_d3d.swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE))) {
        return false;
    }
    ComPtr<ID3D11Texture2D> backbuffer;
    if (FAILED(g_d3d.swap_chain->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf())))) {
        return false;
    }
    if (FAILED(g_d3d.device->CreateRenderTargetView(backbuffer.Get(), nullptr, g_d3d.render_target.GetAddressOf()))) {
        return false;
    }
    D3D11_TEXTURE2D_DESC depth_desc {};
    depth_desc.Width = width;
    depth_desc.Height = height;
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    if (FAILED(g_d3d.device->CreateTexture2D(&depth_desc, nullptr, depth.GetAddressOf()))) {
        return false;
    }
    if (FAILED(g_d3d.device->CreateDepthStencilView(depth.Get(), nullptr, g_d3d.depth_view.GetAddressOf()))) {
        return false;
    }
    g_d3d.width = width;
    g_d3d.height = height;
    return true;
}

bool InitializeD3D(HWND hwnd) {
    RECT client {};
    GetClientRect(hwnd, &client);
    g_d3d.width = static_cast<UINT>(std::max<LONG>(1, client.right - client.left));
    g_d3d.height = static_cast<UINT>(std::max<LONG>(1, client.bottom - client.top));

    DXGI_SWAP_CHAIN_DESC desc {};
    desc.BufferCount = 1;
    desc.BufferDesc.Width = g_d3d.width;
    desc.BufferDesc.Height = g_d3d.height;
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL feature_level {};
    const D3D_FEATURE_LEVEL requested[] {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            requested,
            2,
            D3D11_SDK_VERSION,
            &desc,
            g_d3d.swap_chain.GetAddressOf(),
            g_d3d.device.GetAddressOf(),
            &feature_level,
            g_d3d.context.GetAddressOf()))) {
        return false;
    }

    if (!ResizeBackbuffer(g_d3d.width, g_d3d.height)) {
        return false;
    }

    constexpr const char* kShader = R"(
cbuffer Constants : register(b0) { matrix mvp; };
struct VSIn { float3 pos : POSITION; float4 color : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR; };
VSOut VSMain(VSIn input) {
    VSOut output;
    output.pos = mul(float4(input.pos, 1.0), mvp);
    output.color = input.color;
    return output;
}
float4 PSMain(VSOut input) : SV_TARGET { return input.color; }
)";
    ComPtr<ID3DBlob> vs_blob;
    ComPtr<ID3DBlob> ps_blob;
    if (FAILED(CompileShader(kShader, "VSMain", "vs_4_0", vs_blob.GetAddressOf())) ||
        FAILED(CompileShader(kShader, "PSMain", "ps_4_0", ps_blob.GetAddressOf()))) {
        return false;
    }
    if (FAILED(g_d3d.device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, g_d3d.vertex_shader.GetAddressOf())) ||
        FAILED(g_d3d.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, g_d3d.pixel_shader.GetAddressOf()))) {
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC input_desc[] {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(g_d3d.device->CreateInputLayout(input_desc, 2, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), g_d3d.input_layout.GetAddressOf()))) {
        return false;
    }
    D3D11_BUFFER_DESC cb_desc {};
    cb_desc.ByteWidth = sizeof(ConstantBuffer);
    cb_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(g_d3d.device->CreateBuffer(&cb_desc, nullptr, g_d3d.constant_buffer.GetAddressOf()))) {
        return false;
    }
    return CreateDynamicBuffer(sizeof(Vertex) * 700000, g_d3d.triangle_buffer.GetAddressOf()) &&
        CreateDynamicBuffer(sizeof(Vertex) * 220000, g_d3d.line_buffer.GetAddressOf());
}

void UploadAndDraw(const std::vector<Vertex>& vertices, ID3D11Buffer* buffer, D3D11_PRIMITIVE_TOPOLOGY topology) {
    if (vertices.empty()) {
        return;
    }
    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(g_d3d.context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        return;
    }
    std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    g_d3d.context->Unmap(buffer, 0);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_d3d.context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
    g_d3d.context->IASetPrimitiveTopology(topology);
    g_d3d.context->Draw(static_cast<UINT>(vertices.size()), 0);
}

void PollRuntime() {
    const auto now = std::chrono::steady_clock::now();
    if (g_app.last_poll.time_since_epoch().count() != 0) {
        const float elapsed = std::chrono::duration<float>(now - g_app.last_poll).count();
        if (elapsed < kPollSeconds) {
            return;
        }
    }
    g_app.last_poll = now;
    if (const auto snapshot_text = ReadChangedTextFile(g_app.runtime_dir / "world_snapshot.json", g_app.snapshot_file_cache);
        snapshot_text.has_value()) {
        const auto parsed = ParseSnapshot(*snapshot_text);
        if (!parsed.units.empty() || !parsed.resource_nodes.empty() || !parsed.structures.empty()) {
            g_app.snapshot = parsed;
            RebuildTerrainHeightLookup();
            g_app.frame_ground_height_cache.clear();
            SyncTerrainChunkMeshCache();
            SyncRenderUnitPositionsAfterSnapshot();
            PruneSelectedUnits();
            if (g_app.primary_player_id == 0) {
                for (const auto& unit : g_app.snapshot.units) {
                    if (unit.controller_player_id != 0) {
                        g_app.primary_player_id = unit.controller_player_id;
                        break;
                    }
                }
            }
            if (!g_app.camera_focused_on_units && !g_app.camera_moved_by_user && g_app.primary_player_id != 0) {
                float sum_x = 0.0f;
                float sum_z = 0.0f;
                int count = 0;
                for (const auto& unit : g_app.snapshot.units) {
                    if (!IsCommandableUnit(unit)) {
                        continue;
                    }
                    sum_x += unit.position.x;
                    sum_z += unit.position.z;
                    ++count;
                }
                if (count > 0) {
                    g_app.camera.center_x = sum_x / static_cast<float>(count);
                    g_app.camera.center_z = sum_z / static_cast<float>(count);
                    g_app.camera.distance = 72.0f;
                    g_app.camera_focused_on_units = true;
                    g_app.status_line = L"Camera focused on your starter people.";
                }
            }
        }
    }
    if (const auto status_text = ReadChangedTextFile(g_app.runtime_dir / "session_status.json", g_app.session_file_cache);
        status_text.has_value()) {
        g_app.session = ParseSessionStatus(*status_text);
    }
}

void DrawHudText() {
    ComPtr<IDXGISurface1> surface;
    if (FAILED(g_d3d.swap_chain->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf())))) {
        return;
    }
    HDC hdc {};
    if (FAILED(surface->GetDC(FALSE, &hdc))) {
        return;
    }
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(hdc, font);

    auto draw = [&](int x, int y, const std::wstring& text, COLORREF color) {
        SetTextColor(hdc, color);
        TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
    };
    auto draw_world = [&](const dbd::Vec3& position, float y_offset, const std::wstring& text, COLORREF color) {
        const auto screen = WorldToScreen(position, y_offset);
        if (!screen.has_value()) {
            return;
        }
        draw(screen->x - 18, screen->y - 8, text, color);
    };

    for (const auto& node : g_app.snapshot.resource_nodes) {
        draw_world(node.position, 5.8f, ToWide(ResourcePlaceholder(node)), IsObjectiveNode(node) ? RGB(240, 249, 255) : RGB(190, 242, 100));
    }
    for (const auto& storage : g_app.snapshot.storage_sites) {
        draw_world(storage.position, 7.0f, L"depot", RGB(134, 239, 172));
    }
    for (const auto& structure : g_app.snapshot.structures) {
        draw_world(structure.position, 8.0f, structure.structure_type == "StorageDepot" ? L"depot" : L"structure", RGB(147, 197, 253));
    }
    for (const auto& job : g_app.snapshot.flatten_jobs) {
        draw_world(job.center, 3.2f, L"flatten", RGB(253, 224, 71));
    }
    for (const auto& site : g_app.snapshot.construction_sites) {
        draw_world(site.position, 7.0f, L"site", RGB(251, 146, 60));
    }
    for (const auto& drop : g_app.snapshot.dropped_cargo) {
        draw_world(drop.position, 4.2f, drop.primary_item_name == "Survey Marker" ? L"marker" : L"cargo", RGB(240, 171, 252));
    }
    for (const auto& contact : g_app.snapshot.known_contacts) {
        draw_world(contact.position, 4.8f, contact.freshness == "visible" ? L"contact" : L"stale", contact.freshness == "visible" ? RGB(248, 113, 113) : RGB(251, 191, 36));
    }
    for (const auto& unit : g_app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const bool friendly = IsCommandableUnit(unit);
        std::wstring label = friendly ? L"person" : L"raider";
        const auto objective = ObjectiveCargoState(unit);
        if (!objective.empty()) {
            label = ToWide(objective);
        }
        draw_world(RenderPositionForUnit(unit), 6.8f, label, friendly ? RGB(147, 197, 253) : RGB(252, 165, 165));
    }

    RECT panel {12, 12, 700, 132};
    HBRUSH panel_brush = CreateSolidBrush(RGB(10, 18, 32));
    FillRect(hdc, &panel, panel_brush);
    DeleteObject(panel_brush);
    FrameRect(hdc, &panel, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    std::wostringstream title;
    title << L"DBD Client 3D  |  Units " << g_app.snapshot.units.size()
          << L"  Resources " << g_app.snapshot.resource_nodes.size()
          << L"  Tick " << g_app.session.tick
          << L"  Mode " << CommandModeText();
    draw(24, 20, title.str(), RGB(240, 249, 255));

    if (g_app.session.scenario.present) {
        std::wostringstream scenario;
        scenario << L"Depot Run: Iron "
                 << static_cast<int>(std::round(g_app.session.scenario.iron_fitting_stored))
                 << L"/" << static_cast<int>(std::round(g_app.session.scenario.iron_fitting_goal))
                 << L"  Reason " << ToWide(g_app.session.scenario.reason)
                 << L"  Time " << g_app.session.scenario.remaining_ticks;
        if (g_app.session.scenario.state == "failed" && g_app.session.scenario.reason == "time_expired" && g_app.session.scenario.remaining_ticks > 0) {
            scenario << L"  (timer unavailable)";
        }
        draw(24, 44, scenario.str(), RGB(191, 219, 254));
    }
    {
        const int total_minutes = static_cast<int>(std::round(g_app.snapshot.day_fraction * 24.0f * 60.0f)) % (24 * 60);
        const int hour = total_minutes / 60;
        const int minute = total_minutes % 60;
        std::wostringstream time_line;
        time_line << L"World time " << std::setw(2) << std::setfill(L'0') << hour
                  << L":" << std::setw(2) << std::setfill(L'0') << minute
                  << L"  " << (g_app.snapshot.night_factor >= 0.52f ? L"Moonlight" : L"Daylight");
        draw(24, 68, time_line.str(), RGB(186, 230, 253));
    }
    draw(24, 92, L"WASD camera  RMB move/resource harvest  Tab inventory  1 attack  I/C/H/L/G/Z/F/B modes  Q craft  T use", RGB(196, 181, 253));
    if (!g_app.status_line.empty()) {
        draw(24, 116, g_app.status_line, RGB(148, 163, 184));
    }

    RECT inventory_button {560, 116, 690, 146};
    HBRUSH button_brush = CreateSolidBrush(g_app.inventory_open ? RGB(30, 64, 175) : RGB(30, 41, 59));
    FillRect(hdc, &inventory_button, button_brush);
    DeleteObject(button_brush);
    FrameRect(hdc, &inventory_button, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    draw(inventory_button.left + 12, inventory_button.top + 6, g_app.inventory_open ? L"Inventory ON" : L"Inventory", RGB(240, 249, 255));
    if (!g_app.session.combat_alert.empty()) {
        draw(720, 20, L"Combat: " + ToWide(g_app.session.combat_alert), RGB(252, 165, 165));
    }
    if (!g_app.session.supply_alert.empty()) {
        draw(720, 44, L"Supply: " + ToWide(g_app.session.supply_alert), RGB(251, 191, 36));
    }
    if (!g_app.session.contact_alert.empty()) {
        draw(720, 68, L"Contact: " + ToWide(g_app.session.contact_alert), RGB(216, 180, 254));
    }
    if (!g_app.session.repair_alert.empty()) {
        draw(720, 92, L"Repair: " + ToWide(g_app.session.repair_alert), RGB(134, 239, 172));
    }

    int y = 164;
    std::size_t objective_cargo_units = 0;
    float selected_cargo_weight = 0.0f;
    float selected_carry_capacity = 0.0f;
    for (Id unit_id : g_app.selected_units) {
        const UnitView* unit = FindUnitById(unit_id);
        if (unit == nullptr) {
            continue;
        }
        if (IsObjectiveCargo(*unit)) {
            ++objective_cargo_units;
        }
        selected_cargo_weight += unit->cargo_weight;
        selected_carry_capacity += std::max(0.0f, unit->carry_capacity);
        std::wostringstream line;
        line << L"Selected #" << unit->unit_id
             << L" " << ToWide(unit->order)
             << L" HP " << static_cast<int>(std::round(unit->health))
             << L" STA " << static_cast<int>(std::round(unit->stamina))
             << L" Cargo " << static_cast<int>(std::round(unit->cargo_weight))
             << L"/" << static_cast<int>(std::round(unit->carry_capacity))
             << L" " << ToWide(unit->cargo_primary_item_name)
             << L" Q" << unit->queued_order_count;
        if (unit->route_active) {
            line << L" Route " << ToWide(unit->route_phase) << L"/" << ToWide(unit->route_threat_level);
        }
        draw(18, y, line.str(), IsObjectiveCargo(*unit) ? RGB(96, 165, 250) : RGB(226, 232, 240));
        y += 22;
        const auto objective = ObjectiveCargoState(*unit);
        if (!objective.empty()) {
            draw(18, y, ToWide(objective), objective.find("CRITICAL") != std::string::npos ? RGB(248, 113, 113) : RGB(96, 165, 250));
            y += 22;
        }
    }

    if (g_app.inventory_open) {
        const UnitView* first_unit = g_app.selected_units.empty() ? nullptr : FindUnitById(g_app.selected_units.front());
        RECT inventory_panel {12, static_cast<LONG>(y + 6), 650, static_cast<LONG>(y + 142)};
        HBRUSH inventory_brush = CreateSolidBrush(RGB(15, 23, 42));
        FillRect(hdc, &inventory_panel, inventory_brush);
        DeleteObject(inventory_brush);
        FrameRect(hdc, &inventory_panel, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

        std::wostringstream inv;
        inv << L"Inventory: selected " << g_app.selected_units.size()
            << L" unit(s), weight " << static_cast<int>(std::round(selected_cargo_weight))
            << L"/" << static_cast<int>(std::round(selected_carry_capacity));
        if (objective_cargo_units > 0) {
            inv << L"  objective carriers " << objective_cargo_units;
        }
        draw(inventory_panel.left + 10, inventory_panel.top + 10, inv.str(), RGB(226, 232, 240));

        if (first_unit != nullptr) {
            const StorageSiteView* storage = FindNearestStorage(first_unit->position);
            if (storage != nullptr) {
                std::wostringstream depot;
                depot << L"Nearest depot #" << storage->storage_site_id
                      << L" stacks " << storage->stored_stack_count
                      << L" primary " << ToWide(storage->primary_item_name);
                draw(inventory_panel.left + 10, inventory_panel.top + 34, depot.str(), RGB(134, 239, 172));
                std::wostringstream craft;
                craft << L"Craft: Q crate "
                      << (CanCraftStorageCrate(*storage) ? L"ready" : L"needs wood/iron")
                      << L" | Shift+Q shovel "
                      << (CanCraftFieldShovel(*storage) ? L"ready" : L"needs wood/stone")
                      << L" | T use shovel x" << storage->field_shovel_amount
                      << L" | Shift+B install crate x" << storage->storage_crate_amount;
                draw(inventory_panel.left + 10, inventory_panel.top + 58, craft.str(), RGB(203, 213, 225));
            }
            if (first_unit->cargo_weight > first_unit->carry_capacity) {
                draw(inventory_panel.left + 10, inventory_panel.top + 78, L"Overloaded: movement and retreat are slower.", RGB(251, 191, 36));
            } else if (first_unit->cargo_weight > 0.0f) {
                draw(inventory_panel.left + 10, inventory_panel.top + 78, L"Cargo is unsafe until deposited.", RGB(191, 219, 254));
            }
        } else {
            draw(inventory_panel.left + 10, inventory_panel.top + 34, L"Select a person to inspect carried cargo and nearest depot crafting materials.", RGB(203, 213, 225));
        }
        draw(inventory_panel.left + 10, inventory_panel.top + 102, L"Resource interaction: select person(s), right-click wood/stone/iron to harvest; press R to deposit.", RGB(196, 181, 253));
        y = inventory_panel.bottom + 10;
    }

    if (g_app.mouse_valid) {
        std::wstring hover_title;
        std::wstring hover_detail;
        const int mx = g_app.mouse.x;
        const int my = g_app.mouse.y;
        auto near_mouse = [&](const dbd::Vec3& position, float y_offset, float radius) {
            const auto screen = WorldToScreen(position, y_offset);
            if (!screen.has_value()) {
                return false;
            }
            const float dx = static_cast<float>(screen->x - mx);
            const float dy = static_cast<float>(screen->y - my);
            return (dx * dx + dy * dy) <= radius * radius;
        };

        for (const auto& unit : g_app.snapshot.units) {
            if (near_mouse(RenderPositionForUnit(unit), 4.5f, 28.0f)) {
                hover_title = (IsCommandableUnit(unit) ? L"person #" : L"raider #") + std::to_wstring(unit.unit_id);
                std::wostringstream detail;
                detail << L"HP " << static_cast<int>(std::round(unit.health)) << L"/" << static_cast<int>(std::round(unit.max_health))
                       << L"  cargo " << static_cast<int>(std::round(unit.cargo_weight)) << L"/" << static_cast<int>(std::round(unit.carry_capacity))
                       << L"  " << ToWide(unit.cargo_primary_item_name);
                hover_detail = detail.str();
                break;
            }
        }
        if (hover_title.empty()) {
            for (const auto& node : g_app.snapshot.resource_nodes) {
                if (near_mouse(node.position, 3.0f, 18.0f)) {
                    hover_title = L"resource #" + std::to_wstring(node.resource_node_id);
                    hover_detail = ToWide(node.produces_item_name) + L"  LMB in Harvest mode or RMB on OBJ iron";
                    break;
                }
            }
        }
        if (hover_title.empty()) {
            for (const auto& storage : g_app.snapshot.storage_sites) {
                if (near_mouse(storage.position, 4.0f, 22.0f)) {
                    hover_title = L"depot #" + std::to_wstring(storage.storage_site_id);
                    std::wostringstream detail;
                    detail << L"stored stacks " << storage.stored_stack_count << L"  primary " << ToWide(storage.primary_item_name)
                           << L"  wood " << storage.basic_wood_amount
                           << L" stone " << storage.stone_block_amount
                           << L" iron " << storage.iron_fitting_amount;
                    hover_detail = detail.str();
                    break;
                }
            }
        }
        if (hover_title.empty()) {
            for (const auto& drop : g_app.snapshot.dropped_cargo) {
                if (near_mouse(drop.position, 2.5f, 18.0f)) {
                    hover_title = (drop.primary_item_name == "Survey Marker" ? L"marker #" : L"cargo #") + std::to_wstring(drop.dropped_cargo_id);
                    std::wostringstream detail;
                    detail << ToWide(drop.primary_item_name) << L"  stacks " << drop.stack_count
                           << L"  weight " << static_cast<int>(std::round(drop.total_weight)) << L"  L to loot";
                    hover_detail = detail.str();
                    break;
                }
            }
        }
        if (hover_title.empty()) {
            for (const auto& structure : g_app.snapshot.structures) {
                if (near_mouse(structure.position, 4.5f, 22.0f)) {
                    hover_title = ToWide(structure.structure_type) + L" #" + std::to_wstring(structure.structure_id);
                    std::wostringstream detail;
                    detail << L"HP " << static_cast<int>(std::round(structure.health))
                           << L"/" << static_cast<int>(std::round(structure.max_health))
                           << L"  A attack  G guard  Z repair";
                    hover_detail = detail.str();
                    break;
                }
            }
        }
        if (hover_title.empty()) {
            for (const auto& site : g_app.snapshot.construction_sites) {
                if (near_mouse(site.position, 4.0f, 24.0f)) {
                    hover_title = L"construction site #" + std::to_wstring(site.construction_site_id);
                    std::wostringstream detail;
                    detail << ToWide(site.structure_type) << L"  HP " << static_cast<int>(std::round(site.health))
                           << L"/" << static_cast<int>(std::round(site.max_health))
                           << L"  progress " << static_cast<int>(std::round(site.completion_ratio * 100.0f)) << L"%";
                    hover_detail = detail.str();
                    break;
                }
            }
        }
        if (!hover_title.empty()) {
            RECT hover {mx + 18, my + 18, mx + 390, my + 82};
            HBRUSH hover_brush = CreateSolidBrush(RGB(2, 6, 23));
            FillRect(hdc, &hover, hover_brush);
            DeleteObject(hover_brush);
            FrameRect(hdc, &hover, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            draw(hover.left + 10, hover.top + 8, hover_title, RGB(240, 249, 255));
            draw(hover.left + 10, hover.top + 32, hover_detail, RGB(203, 213, 225));
        }
    }

    if (g_app.session.scenario.present && (g_app.session.scenario.state == "success" || g_app.session.scenario.state == "failed")) {
        const LONG banner_center = static_cast<LONG>(g_d3d.width / 2);
        RECT banner {banner_center - 230, 24, banner_center + 230, 88};
        HBRUSH brush = CreateSolidBrush(g_app.session.scenario.state == "success" ? RGB(22, 101, 52) : RGB(127, 29, 29));
        FillRect(hdc, &banner, brush);
        DeleteObject(brush);
        FrameRect(hdc, &banner, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        draw(banner.left + 16, banner.top + 12, g_app.session.scenario.state == "success" ? L"DEPOT RUN COMPLETE" : L"DEPOT RUN FAILED", RGB(240, 249, 255));
        draw(banner.left + 16, banner.top + 36, L"Reason: " + ToWide(g_app.session.scenario.reason), RGB(226, 232, 240));
    }

    SelectObject(hdc, old_font);
    DeleteObject(font);
    surface->ReleaseDC(nullptr);
}

void Render() {
    PollRuntime();
    g_app.frame_ground_height_cache.clear();
    const float night = std::clamp(g_app.snapshot.night_factor, 0.0f, 1.0f);
    FLOAT clear[4] {
        0.025f + (1.0f - night) * 0.035f,
        0.035f + (1.0f - night) * 0.08f,
        0.055f + (1.0f - night) * 0.12f,
        1.0f
    };
    g_d3d.context->OMSetRenderTargets(1, g_d3d.render_target.GetAddressOf(), g_d3d.depth_view.Get());
    g_d3d.context->ClearRenderTargetView(g_d3d.render_target.Get(), clear);
    g_d3d.context->ClearDepthStencilView(g_d3d.depth_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    D3D11_VIEWPORT viewport {0.0f, 0.0f, static_cast<float>(g_d3d.width), static_cast<float>(g_d3d.height), 0.0f, 1.0f};
    g_d3d.context->RSSetViewports(1, &viewport);
    g_d3d.context->IASetInputLayout(g_d3d.input_layout.Get());
    g_d3d.context->VSSetShader(g_d3d.vertex_shader.Get(), nullptr, 0);
    g_d3d.context->PSSetShader(g_d3d.pixel_shader.Get(), nullptr, 0);

    ConstantBuffer constants {};
    constants.mvp = XMMatrixTranspose(ViewMatrix() * ProjectionMatrix());
    g_d3d.context->UpdateSubresource(g_d3d.constant_buffer.Get(), 0, nullptr, &constants, 0, 0);
    g_d3d.context->VSSetConstantBuffers(0, 1, g_d3d.constant_buffer.GetAddressOf());

    auto& tris = g_app.frame_triangles;
    auto& lines = g_app.frame_lines;
    tris.clear();
    lines.clear();
    if (tris.capacity() < 420000) {
        tris.reserve(420000);
    }
    if (lines.capacity() < 12000) {
        lines.reserve(12000);
    }

    const float lunar_angle = (g_app.snapshot.day_fraction + 0.18f) * XM_2PI;
    const XMFLOAT3 moon_position {
        g_app.camera.center_x + std::cos(lunar_angle) * 88.0f,
        64.0f + std::max(0.0f, std::sin(lunar_angle)) * 34.0f,
        g_app.camera.center_z + std::sin(lunar_angle) * 88.0f
    };
    AddOctahedron(tris, moon_position, 4.2f + night * 1.6f, Tint(Color(0.82f, 0.88f, 1.0f), 0.82f + night * 0.22f));

    AppendCachedTerrainVertices(tris);

    for (const auto& chunk : g_app.snapshot.chunks) {
        if (!chunk.loaded) {
            continue;
        }
        const XMFLOAT4 yellow = Color(1.0f, 0.88f, 0.12f);
        const float y = 0.22f;
        AddLine(lines, {chunk.min.x, y, chunk.min.z}, {chunk.max.x, y, chunk.min.z}, yellow);
        AddLine(lines, {chunk.max.x, y, chunk.min.z}, {chunk.max.x, y, chunk.max.z}, yellow);
        AddLine(lines, {chunk.max.x, y, chunk.max.z}, {chunk.min.x, y, chunk.max.z}, yellow);
        AddLine(lines, {chunk.min.x, y, chunk.max.z}, {chunk.min.x, y, chunk.min.z}, yellow);
    }

    for (const auto& node : g_app.snapshot.resource_nodes) {
        AddProjectedShadow(tris, node.position, 1.5f, 1.4f, night);
        AddVoxelResource(tris, node);
        if (IsObjectiveNode(node)) {
            AddCircle(lines, WorldPoint(node.position, 0.08f), 5.2f, Color(0.92f, 0.97f, 1.0f));
            AddLine(lines, WorldPoint(node.position, 0.0f), WorldPoint(node.position, 9.0f), Color(0.92f, 0.97f, 1.0f));
        }
    }

    for (const auto& storage : g_app.snapshot.storage_sites) {
        AddProjectedShadow(tris, storage.position, 3.5f, 2.1f, night);
        AddVoxelDepot(tris, storage.position);
    }
    for (const auto& job : g_app.snapshot.flatten_jobs) {
        AddCircle(lines, WorldPoint(job.center, 0.1f), job.radius, Color(0.98f, 0.82f, 0.22f));
        const float progress = job.required_labor <= 0.0f ? 0.0f : std::clamp(job.accumulated_labor / job.required_labor, 0.0f, 1.0f);
        AddLine(lines, WorldPoint(job.center, 0.15f), {job.center.x + (job.radius * progress), 0.15f, job.center.z}, Color(0.98f, 0.82f, 0.22f));
    }
    for (const auto& structure : g_app.snapshot.structures) {
        const bool damaged = structure.max_health > 0.0f && structure.health / structure.max_health <= 0.45f;
        AddProjectedShadow(tris, structure.position, 3.1f, 2.4f, night);
        AddVoxelStructure(tris, structure.position, damaged);
        if (damaged) {
            const float wire_ground = StableFootprintGroundHeight(structure.position.x, structure.position.z, 2.8f);
            AddBoxWire(lines, {structure.position.x, wire_ground + 3.2f, structure.position.z}, {5.4f, 3.6f, 5.4f}, Color(1.0f, 0.18f, 0.18f));
        }
    }
    for (const auto& site : g_app.snapshot.construction_sites) {
        AddProjectedShadow(tris, site.position, std::max(1.8f, site.footprint_radius * 0.42f), 1.1f, night);
        const float site_wire_ground = StableFootprintGroundHeight(site.position.x, site.position.z, std::max(1.8f, site.footprint_radius * 0.38f));
        AddBoxWire(lines, {site.position.x, site_wire_ground + 2.4f, site.position.z}, {site.footprint_radius, 2.4f, site.footprint_radius}, Color(1.0f, 0.55f, 0.18f));
        AddVoxelConstructionSite(tris, site);
    }
    for (const auto& drop : g_app.snapshot.dropped_cargo) {
        AddProjectedShadow(tris, drop.position, 1.0f, 0.8f, night);
        AddVoxelCargo(tris, drop);
    }

    for (const auto& unit : g_app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const dbd::Vec3 render_position = RenderPositionForUnit(unit);
        const bool friendly = IsCommandableUnit(unit);
        const bool selected = IsSelected(unit.unit_id);
        AddProjectedShadow(tris, render_position, 1.0f, 1.2f, night);
        AddVoxelPerson(tris, render_position, friendly, IsObjectiveCargo(unit));
        if (selected) {
            AddCircle(lines, WorldPoint(render_position, 0.12f), 5.0f, Color(1.0f, 1.0f, 1.0f));
        } else if (friendly) {
            AddCircle(lines, WorldPoint(render_position, 0.1f), 3.8f, Color(0.32f, 0.64f, 1.0f));
        }
        if (unit.order == "AttackTarget") {
            AddCircle(lines, WorldPoint(render_position, 0.18f), 5.0f, Color(1.0f, 0.25f, 0.25f));
        }
    }

    UploadAndDraw(tris, g_d3d.triangle_buffer.Get(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UploadAndDraw(lines, g_d3d.line_buffer.Get(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    g_d3d.context->OMSetRenderTargets(0, nullptr, nullptr);
    DrawHudText();
    g_d3d.swap_chain->Present(1, 0);
}

bool ScreenRay(int x, int y, XMVECTOR& ray_origin, XMVECTOR& ray_dir) {
    if (g_d3d.width == 0 || g_d3d.height == 0) {
        return false;
    }
    const float px = (2.0f * static_cast<float>(x) / static_cast<float>(g_d3d.width)) - 1.0f;
    const float py = 1.0f - (2.0f * static_cast<float>(y) / static_cast<float>(g_d3d.height));
    const XMMATRIX inv = XMMatrixInverse(nullptr, ViewMatrix() * ProjectionMatrix());
    const XMVECTOR near_point = XMVector3TransformCoord(XMVectorSet(px, py, 0.0f, 1.0f), inv);
    const XMVECTOR far_point = XMVector3TransformCoord(XMVectorSet(px, py, 1.0f, 1.0f), inv);
    ray_origin = near_point;
    ray_dir = XMVector3Normalize(far_point - near_point);
    return true;
}

std::optional<dbd::Vec3> PickGround(int x, int y) {
    XMVECTOR origin;
    XMVECTOR dir;
    if (!ScreenRay(x, y, origin, dir)) {
        return std::nullopt;
    }
    const float oy = XMVectorGetY(origin);
    const float dy = XMVectorGetY(dir);
    if (std::abs(dy) < 0.0001f) {
        return std::nullopt;
    }
    const float t = -oy / dy;
    if (t < 0.0f) {
        return std::nullopt;
    }
    const XMVECTOR hit = origin + dir * t;
    return dbd::Vec3 {XMVectorGetX(hit), 0.0f, XMVectorGetZ(hit)};
}

std::optional<Id> PickUnit(int x, int y) {
    float best_screen_distance_sq = 1.0e9f;
    std::optional<Id> best_screen;
    for (const auto& unit : g_app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const auto screen = WorldToScreen(RenderPositionForUnit(unit), 4.2f);
        if (!screen.has_value()) {
            continue;
        }
        const float dx = static_cast<float>(screen->x - x);
        const float dy = static_cast<float>(screen->y - y);
        const float distance_sq = (dx * dx) + (dy * dy);
        if (distance_sq <= (34.0f * 34.0f) && distance_sq < best_screen_distance_sq) {
            best_screen_distance_sq = distance_sq;
            best_screen = unit.unit_id;
        }
    }
    if (best_screen.has_value()) {
        return best_screen;
    }

    XMVECTOR origin;
    XMVECTOR dir;
    if (!ScreenRay(x, y, origin, dir)) {
        return std::nullopt;
    }
    float best_t = 1.0e9f;
    std::optional<Id> best;
    for (const auto& unit : g_app.snapshot.units) {
        if (!unit.alive) {
            continue;
        }
        const dbd::Vec3 render_position = RenderPositionForUnit(unit);
        const XMVECTOR center = XMVectorSet(render_position.x, 2.7f, render_position.z, 1.0f);
        const XMVECTOR oc = origin - center;
        const float b = XMVectorGetX(XMVector3Dot(oc, dir));
        const float c = XMVectorGetX(XMVector3Dot(oc, oc)) - (4.6f * 4.6f);
        const float disc = (b * b) - c;
        if (disc < 0.0f) {
            continue;
        }
        const float t = -b - std::sqrt(disc);
        if (t > 0.0f && t < best_t) {
            best_t = t;
            best = unit.unit_id;
        }
    }
    return best;
}

template <typename Range, typename GetId, typename GetPosition>
std::optional<Id> PickByRadius(int x, int y, const Range& range, GetId get_id, GetPosition get_position, float radius) {
    XMVECTOR origin;
    XMVECTOR dir;
    if (!ScreenRay(x, y, origin, dir)) {
        return std::nullopt;
    }
    float best_t = 1.0e9f;
    std::optional<Id> best;
    for (const auto& item : range) {
        const auto position = get_position(item);
        const XMVECTOR center = XMVectorSet(position.x, 2.0f, position.z, 1.0f);
        const XMVECTOR oc = origin - center;
        const float b = XMVectorGetX(XMVector3Dot(oc, dir));
        const float c = XMVectorGetX(XMVector3Dot(oc, oc)) - (radius * radius);
        const float disc = (b * b) - c;
        if (disc < 0.0f) {
            continue;
        }
        const float t = -b - std::sqrt(disc);
        if (t > 0.0f && t < best_t) {
            best_t = t;
            best = get_id(item);
        }
    }
    return best;
}

std::optional<Id> PickResourceNode(int x, int y) {
    return PickByRadius(x, y, g_app.snapshot.resource_nodes, [](const ResourceNodeView& node) { return node.resource_node_id; }, [](const ResourceNodeView& node) { return node.position; }, 4.2f);
}

std::optional<Id> PickStorageSite(int x, int y) {
    return PickByRadius(x, y, g_app.snapshot.storage_sites, [](const StorageSiteView& storage) { return storage.storage_site_id; }, [](const StorageSiteView& storage) { return storage.position; }, 5.0f);
}

std::optional<Id> PickStructure(int x, int y) {
    return PickByRadius(x, y, g_app.snapshot.structures, [](const StructureView& structure) { return structure.structure_id; }, [](const StructureView& structure) { return structure.position; }, 5.2f);
}

std::optional<Id> PickConstructionSite(int x, int y) {
    return PickByRadius(x, y, g_app.snapshot.construction_sites, [](const ConstructionSiteView& site) { return site.construction_site_id; }, [](const ConstructionSiteView& site) { return site.position; }, 6.0f);
}

std::optional<Id> PickDroppedCargo(int x, int y) {
    return PickByRadius(x, y, g_app.snapshot.dropped_cargo, [](const DroppedCargoView& drop) { return drop.dropped_cargo_id; }, [](const DroppedCargoView& drop) { return drop.position; }, 3.5f);
}

std::optional<Id> ResourceNodeAt(const dbd::Vec3& point, float radius = 6.0f) {
    const float radius_sq = radius * radius;
    std::optional<Id> best_id;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto& node : g_app.snapshot.resource_nodes) {
        const float distance = DistanceSq2D(point, node.position);
        if (distance <= radius_sq && distance < best_distance) {
            best_distance = distance;
            best_id = node.resource_node_id;
        }
    }
    return best_id;
}

std::optional<Id> ObjectiveNodeAt(const dbd::Vec3& point) {
    for (const auto& node : g_app.snapshot.resource_nodes) {
        if (!IsObjectiveNode(node)) {
            continue;
        }
        if (DistanceSq2D(point, node.position) <= 6.0f * 6.0f) {
            return node.resource_node_id;
        }
    }
    return std::nullopt;
}

std::string MakeCommandId(const std::string& prefix) {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::ostringstream out;
    out << "client3d-" << prefix << "-" << millis << "-" << (++g_app.command_sequence);
    return out.str();
}

bool WriteCommand(const std::string& command_id, const std::string& json) {
    const fs::path inbox = g_app.runtime_dir / "command_spool" / "inbox";
    std::error_code ec;
    fs::create_directories(inbox, ec);
    const fs::path tmp = inbox / (command_id + ".tmp");
    const fs::path final = inbox / (command_id + ".json");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << json;
    }
    fs::rename(tmp, final, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

std::string SelectedUnitJsonArray() {
    PruneSelectedUnits();
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < g_app.selected_units.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << g_app.selected_units[i];
    }
    out << "]";
    return out.str();
}

std::string VecJson(const dbd::Vec3& value) {
    std::ostringstream out;
    out << "{\"x\":" << value.x << ",\"y\":" << value.y << ",\"z\":" << value.z << "}";
    return out.str();
}

void IssueUnitListCommand(const std::string& type, const std::string& extra_json = "", const std::wstring& label = L"command") {
    if (g_app.selected_units.empty() || g_app.primary_player_id == 0) {
        g_app.status_line = L"Select units first.";
        return;
    }
    const auto command_id = MakeCommandId(type);
    std::ostringstream json;
    json << "{\"commandId\":\"" << JsonEscape(command_id) << "\",\"type\":\"" << JsonEscape(type) << "\""
         << ",\"controllerPlayerId\":" << g_app.primary_player_id
         << ",\"unitIds\":" << SelectedUnitJsonArray();
    if (!extra_json.empty()) {
        json << "," << extra_json;
    }
    json << "}";
    g_app.status_line = WriteCommand(command_id, json.str()) ? (label + L" sent.") : (L"Failed to write " + label + L".");
}

void IssuePerUnitCommand(const std::string& type, const std::string& extra_json, const std::wstring& label) {
    if (g_app.selected_units.empty() || g_app.primary_player_id == 0) {
        g_app.status_line = L"Select units first.";
        return;
    }
    const auto command_id = MakeCommandId(type);
    std::ostringstream json;
    json << "{\"commands\":[";
    for (std::size_t i = 0; i < g_app.selected_units.size(); ++i) {
        if (i != 0) {
            json << ",";
        }
        json << "{\"commandId\":\"" << JsonEscape(command_id) << "-" << (i + 1) << "\",\"type\":\"" << JsonEscape(type) << "\""
             << ",\"controllerPlayerId\":" << g_app.primary_player_id
             << ",\"unitId\":" << g_app.selected_units[i];
        if (!extra_json.empty()) {
            json << "," << extra_json;
        }
        json << "}";
    }
    json << "]}";
    g_app.status_line = WriteCommand(command_id, json.str()) ? (label + L" sent.") : (L"Failed to write " + label + L".");
}

void IssueFormationMove(const dbd::Vec3& target) {
    if (g_app.selected_units.empty() || g_app.primary_player_id == 0) {
        g_app.status_line = L"Select units first.";
        return;
    }
    const auto command_id = MakeCommandId("formation_move");
    std::ostringstream json;
    json << "{\"commandId\":\"" << JsonEscape(command_id) << "\",\"type\":\"formation_move\""
         << ",\"controllerPlayerId\":" << g_app.primary_player_id
         << ",\"unitIds\":" << SelectedUnitJsonArray()
         << ",\"target\":{\"x\":" << target.x << ",\"y\":0,\"z\":" << target.z << "}}";
    g_app.status_line = WriteCommand(command_id, json.str()) ? L"formation_move sent." : L"Failed to write formation_move.";
}

void IssueQueueMove(const dbd::Vec3& target) {
    IssueUnitListCommand("queue_move", "\"target\":" + VecJson(target), L"queue_move");
}

void IssueHarvest(Id node_id) {
    if (g_app.selected_units.empty() || g_app.primary_player_id == 0 || node_id == 0) {
        g_app.status_line = L"Select units before harvesting OBJ.";
        return;
    }
    const auto command_id = MakeCommandId("harvest");
    std::ostringstream json;
    json << "{\"commands\":[";
    for (std::size_t i = 0; i < g_app.selected_units.size(); ++i) {
        if (i != 0) {
            json << ",";
        }
        json << "{\"commandId\":\"" << JsonEscape(command_id) << "-" << (i + 1) << "\",\"type\":\"harvest\""
             << ",\"controllerPlayerId\":" << g_app.primary_player_id
             << ",\"unitId\":" << g_app.selected_units[i]
             << ",\"resourceNodeId\":" << node_id << "}";
    }
    json << "]}";
    g_app.status_line = WriteCommand(command_id, json.str()) ? L"OBJ harvest sent." : L"Failed to write harvest.";
}

void IssueAttack(Id target_id, const std::string& target_kind = "Unit") {
    IssueUnitListCommand("focus_fire", "\"targetKind\":\"" + target_kind + "\",\"targetEntityId\":" + std::to_string(target_id), L"focus_fire");
}

void IssueIntercept(Id target_id) {
    IssueUnitListCommand("intercept_unit", "\"targetEntityId\":" + std::to_string(target_id), L"intercept");
}

void IssueGuardUnit(Id target_id) {
    IssueUnitListCommand("guard_unit", "\"targetEntityId\":" + std::to_string(target_id), L"guard_unit");
}

void IssueGuardSite(Id target_id, const std::string& target_kind) {
    IssueUnitListCommand("guard_site", "\"targetKind\":\"" + target_kind + "\",\"targetEntityId\":" + std::to_string(target_id), L"guard_site");
}

void IssueHold(const dbd::Vec3& position) {
    IssueUnitListCommand("hold_position", "\"target\":" + VecJson(position), L"hold_position");
}

void IssueLoot(Id drop_id) {
    IssueUnitListCommand("loot", "\"droppedCargoId\":" + std::to_string(drop_id), L"loot");
}

void IssueHaulRoute(const std::string& source_kind, Id source_id, Id storage_id = 0) {
    std::string extra = "\"sourceKind\":\"" + source_kind + "\",\"sourceId\":" + std::to_string(source_id);
    if (storage_id != 0) {
        extra += ",\"storageSiteId\":" + std::to_string(storage_id);
    }
    IssueUnitListCommand("haul_route", extra, L"haul_route");
}

void IssueReturnToStorage() {
    IssuePerUnitCommand("return_to_storage", "", L"return_to_storage");
}

void IssueFlatten(const dbd::Vec3& center) {
    std::ostringstream extra;
    extra << "\"center\":" << VecJson(center) << ",\"radius\":" << g_app.pending_flatten_radius << ",\"targetGrade\":0.12";
    IssueUnitListCommand("start_flatten", extra.str(), L"start_flatten");
}

void IssueConstruction(const dbd::Vec3& position) {
    IssueUnitListCommand("start_construction", "\"structureType\":\"StorageDepot\",\"position\":" + VecJson(position) + ",\"footprintRadius\":8", L"start_construction");
}

void IssueCraftItem(Id item_id, std::uint32_t amount, const std::wstring& label) {
    IssueUnitListCommand("craft_item", "\"itemId\":" + std::to_string(item_id) + ",\"amount\":" + std::to_string(amount), label);
}

void IssueInstallItem(Id item_id, const dbd::Vec3& position, const std::wstring& label) {
    IssueUnitListCommand("install_item", "\"itemId\":" + std::to_string(item_id) + ",\"position\":" + VecJson(position), label);
}

void IssueUseItem(Id item_id, const std::wstring& label) {
    IssueUnitListCommand("use_item", "\"itemId\":" + std::to_string(item_id), label);
}

void IssueRepairStructure(Id target_id) {
    IssueUnitListCommand("repair_structure", "\"targetEntityId\":" + std::to_string(target_id), L"repair_structure");
}

void IssueRepairConstructionSite(Id target_id) {
    IssueUnitListCommand("repair_construction_site", "\"targetEntityId\":" + std::to_string(target_id), L"repair_site");
}

void IssueSupplyRepair(Id target_id, const std::string& target_kind) {
    IssueUnitListCommand("supply_repair", "\"targetKind\":\"" + target_kind + "\",\"targetEntityId\":" + std::to_string(target_id), L"supply_repair");
}

void IssueScout(const dbd::Vec3& center) {
    IssueUnitListCommand(g_app.queue_next_order ? "queue_scout_area" : "scout_area", "\"center\":" + VecJson(center) + ",\"radius\":18", g_app.queue_next_order ? L"queue_scout" : L"scout");
    g_app.queue_next_order = false;
}

void IssuePatrol(const dbd::Vec3& point_b) {
    if (!g_app.pending_patrol_start_set) {
        g_app.pending_patrol_start = point_b;
        g_app.pending_patrol_start_set = true;
        g_app.status_line = L"Patrol point A set. Click point B.";
        return;
    }
    IssueUnitListCommand(g_app.queue_next_order ? "queue_patrol_route" : "patrol_route",
        "\"pointA\":" + VecJson(g_app.pending_patrol_start) + ",\"pointB\":" + VecJson(point_b),
        g_app.queue_next_order ? L"queue_patrol" : L"patrol");
    g_app.pending_patrol_start_set = false;
    g_app.queue_next_order = false;
}

void IssueInvestigateContact() {
    if (g_app.selected_contact_id == 0 && !g_app.snapshot.known_contacts.empty()) {
        g_app.selected_contact_id = g_app.snapshot.known_contacts.front().target_entity_id;
        g_app.selected_contact_kind = g_app.snapshot.known_contacts.front().target_kind;
    }
    if (g_app.selected_contact_id == 0) {
        g_app.status_line = L"No contact selected.";
        return;
    }
    IssueUnitListCommand("investigate_contact", "\"targetKind\":\"" + g_app.selected_contact_kind + "\",\"targetEntityId\":" + std::to_string(g_app.selected_contact_id), L"investigate_contact");
}

void SelectNextContact(int direction) {
    if (g_app.snapshot.known_contacts.empty()) {
        g_app.selected_contact_id = 0;
        g_app.status_line = L"No contacts.";
        return;
    }
    std::size_t index = 0;
    for (std::size_t i = 0; i < g_app.snapshot.known_contacts.size(); ++i) {
        if (g_app.snapshot.known_contacts[i].target_entity_id == g_app.selected_contact_id) {
            index = i;
            break;
        }
    }
    if (direction >= 0) {
        index = (index + 1) % g_app.snapshot.known_contacts.size();
    } else {
        index = (index + g_app.snapshot.known_contacts.size() - 1) % g_app.snapshot.known_contacts.size();
    }
    g_app.selected_contact_id = g_app.snapshot.known_contacts[index].target_entity_id;
    g_app.selected_contact_kind = g_app.snapshot.known_contacts[index].target_kind;
    g_app.status_line = L"Selected contact #" + std::to_wstring(g_app.selected_contact_id);
}

bool SelectFriendlyUnit(Id unit_id, bool additive) {
    const UnitView* unit = FindUnitById(unit_id);
    if (unit == nullptr || !IsCommandableUnit(*unit)) {
        g_app.status_line = L"That unit is not yours to command.";
        return false;
    }
    if (!additive) {
        g_app.selected_units.clear();
    }
    const auto existing = std::find(g_app.selected_units.begin(), g_app.selected_units.end(), unit_id);
    if (existing == g_app.selected_units.end()) {
        g_app.selected_units.push_back(unit_id);
        g_app.status_line = L"Selected person #" + std::to_wstring(unit_id);
    } else if (additive) {
        g_app.selected_units.erase(existing);
        g_app.status_line = L"Removed person #" + std::to_wstring(unit_id) + L" from selection.";
    } else {
        g_app.status_line = L"Selected person #" + std::to_wstring(unit_id);
    }
    return true;
}

void OnLeftClick(int x, int y, bool additive) {
    const auto picked_unit = PickUnit(x, y);
    if (picked_unit.has_value()) {
        const UnitView* clicked_unit = FindUnitById(*picked_unit);
        const bool clicked_friendly = clicked_unit != nullptr && IsCommandableUnit(*clicked_unit);
        const bool should_select_friendly = clicked_friendly;
        if (should_select_friendly) {
            SelectFriendlyUnit(*picked_unit, additive);
            return;
        }
    }

    if (g_app.command_mode != CommandMode::Move) {
        const auto ground = PickGround(x, y).value_or(dbd::Vec3 {});
        if (g_app.command_mode == CommandMode::Attack) {
            if (picked_unit.has_value()) {
                IssueAttack(*picked_unit, "Unit");
            } else if (const auto site = PickConstructionSite(x, y); site.has_value()) {
                IssueAttack(*site, "ConstructionSite");
            } else if (const auto structure = PickStructure(x, y); structure.has_value()) {
                IssueAttack(*structure, "Structure");
            }
            return;
        }
        if (g_app.command_mode == CommandMode::Intercept) {
            if (picked_unit.has_value()) {
                IssueIntercept(*picked_unit);
            }
            return;
        }
        if (g_app.command_mode == CommandMode::Harvest) {
            if (const auto node = PickResourceNode(x, y); node.has_value()) {
                IssueHarvest(*node);
            }
            return;
        }
        if (g_app.command_mode == CommandMode::Loot) {
            if (const auto drop = PickDroppedCargo(x, y); drop.has_value()) {
                IssueLoot(*drop);
            }
            return;
        }
        if (g_app.command_mode == CommandMode::HaulRoute) {
            if (!g_app.pending_haul_source_set) {
                if (const auto node = PickResourceNode(x, y); node.has_value()) {
                    g_app.pending_haul_source_kind = "ResourceNode";
                    g_app.pending_haul_source_id = *node;
                    g_app.pending_haul_source_set = true;
                    g_app.status_line = L"Haul source node set. Click depot or ground for nearest.";
                } else if (const auto drop = PickDroppedCargo(x, y); drop.has_value()) {
                    g_app.pending_haul_source_kind = "DroppedCargo";
                    g_app.pending_haul_source_id = *drop;
                    g_app.pending_haul_source_set = true;
                    g_app.status_line = L"Haul source cargo set. Click depot or ground for nearest.";
                }
            } else {
                const Id storage_id = PickStorageSite(x, y).value_or(0);
                IssueHaulRoute(g_app.pending_haul_source_kind, g_app.pending_haul_source_id, storage_id);
                g_app.pending_haul_source_set = false;
            }
            return;
        }
        if (g_app.command_mode == CommandMode::Guard) {
            if (picked_unit.has_value()) {
                IssueGuardUnit(*picked_unit);
            } else if (const auto site = PickConstructionSite(x, y); site.has_value()) {
                IssueGuardSite(*site, "ConstructionSite");
            } else if (const auto structure = PickStructure(x, y); structure.has_value()) {
                IssueGuardSite(*structure, "Structure");
            } else {
                IssueHold(ground);
            }
            return;
        }
        if (g_app.command_mode == CommandMode::Repair || g_app.command_mode == CommandMode::RepairSupply) {
            if (const auto site = PickConstructionSite(x, y); site.has_value()) {
                if (g_app.command_mode == CommandMode::RepairSupply) {
                    IssueSupplyRepair(*site, "ConstructionSite");
                } else {
                    IssueRepairConstructionSite(*site);
                }
            } else if (const auto structure = PickStructure(x, y); structure.has_value()) {
                if (g_app.command_mode == CommandMode::RepairSupply) {
                    IssueSupplyRepair(*structure, "Structure");
                } else {
                    IssueRepairStructure(*structure);
                }
            }
            return;
        }
        if (g_app.command_mode == CommandMode::ScoutArea) {
            IssueScout(ground);
            return;
        }
        if (g_app.command_mode == CommandMode::PatrolRoute) {
            IssuePatrol(ground);
            return;
        }
        if (g_app.command_mode == CommandMode::Flatten) {
            IssueFlatten(ground);
            return;
        }
        if (g_app.command_mode == CommandMode::Build) {
            IssueConstruction(ground);
            return;
        }
        if (g_app.command_mode == CommandMode::Install) {
            IssueInstallItem(92'005, ground, L"install_storage_crate");
            return;
        }
    }

    if (!picked_unit.has_value()) {
        if (!additive) {
            g_app.selected_units.clear();
        }
        return;
    }
    SelectFriendlyUnit(*picked_unit, additive);
}

void OnRightClick(int x, int y, bool queue_move) {
    if (!queue_move) {
        if (const auto node = PickResourceNode(x, y); node.has_value()) {
            IssueHarvest(*node);
            return;
        }
    }
    const auto ground = PickGround(x, y);
    if (!ground.has_value()) {
        return;
    }
    if (const auto resource_node = ResourceNodeAt(*ground); resource_node.has_value() && !queue_move) {
        IssueHarvest(*resource_node);
    } else if (queue_move) {
        IssueQueueMove(*ground);
    } else {
        IssueFormationMove(*ground);
    }
}

void UpdateCameraFromKeys(float dt) {
    const float speed = 52.0f * dt * (g_app.camera.distance / 95.0f);
    bool moved = false;
    if (GetAsyncKeyState('W') & 0x8000) {
        g_app.camera.center_z += speed;
        moved = true;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        g_app.camera.center_z -= speed;
        moved = true;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        g_app.camera.center_x -= speed;
        moved = true;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        g_app.camera.center_x += speed;
        moved = true;
    }
    if (moved) {
        g_app.camera_moved_by_user = true;
    }
}

bool IsInventoryButtonClick(int x, int y) {
    return x >= 560 && x <= 690 && y >= 92 && y <= 122;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_SIZE: {
            if (g_d3d.swap_chain) {
                const UINT width = LOWORD(l_param);
                const UINT height = HIWORD(l_param);
                if (width > 0 && height > 0) {
                    ResizeBackbuffer(width, height);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            if (IsInventoryButtonClick(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param))) {
                g_app.inventory_open = !g_app.inventory_open;
                g_app.status_line = g_app.inventory_open ? L"Inventory panel opened." : L"Inventory panel closed.";
                return 0;
            }
            const bool additive = (GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000);
            OnLeftClick(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param), additive);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            SetFocus(hwnd);
            const bool queue_move = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            OnRightClick(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param), queue_move);
            return 0;
        }
        case WM_MOUSEMOVE: {
            g_app.mouse.x = GET_X_LPARAM(l_param);
            g_app.mouse.y = GET_Y_LPARAM(l_param);
            g_app.mouse_valid = true;
            return 0;
        }
        case WM_KEYDOWN: {
            const bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            switch (w_param) {
                case 'M': g_app.command_mode = CommandMode::Move; g_app.queue_next_order = false; break;
                case VK_TAB:
                    g_app.inventory_open = !g_app.inventory_open;
                    g_app.status_line = g_app.inventory_open ? L"Inventory panel opened." : L"Inventory panel closed.";
                    break;
                case '1': g_app.command_mode = CommandMode::Attack; g_app.queue_next_order = false; break;
                case 'A': g_app.status_line = L"A pans the camera. Press 1 for attack mode."; break;
                case 'I': g_app.command_mode = CommandMode::Intercept; g_app.queue_next_order = false; break;
                case 'C': g_app.command_mode = CommandMode::Harvest; g_app.queue_next_order = false; break;
                case 'L': g_app.command_mode = CommandMode::Loot; g_app.queue_next_order = false; break;
                case 'H':
                    g_app.command_mode = CommandMode::HaulRoute;
                    g_app.pending_haul_source_set = false;
                    g_app.queue_next_order = false;
                    break;
                case 'G': g_app.command_mode = CommandMode::Guard; g_app.queue_next_order = false; break;
                case 'Z': g_app.command_mode = shift_down ? CommandMode::RepairSupply : CommandMode::Repair; g_app.queue_next_order = false; break;
                case 'O': g_app.command_mode = CommandMode::ScoutArea; g_app.queue_next_order = shift_down; break;
                case 'P':
                    g_app.command_mode = CommandMode::PatrolRoute;
                    g_app.queue_next_order = shift_down;
                    g_app.pending_patrol_start_set = false;
                    break;
                case 'F': g_app.command_mode = CommandMode::Flatten; g_app.queue_next_order = false; break;
                case 'B': g_app.command_mode = shift_down ? CommandMode::Install : CommandMode::Build; g_app.queue_next_order = false; break;
                case 'Q': IssueCraftItem(shift_down ? 92'001 : 92'005, 1, shift_down ? L"craft_shovel" : L"craft_storage_crate"); break;
                case 'T': IssueUseItem(shift_down ? 92'006 : 92'001, shift_down ? L"use_hammer" : L"use_shovel"); break;
                case 'R': IssueReturnToStorage(); break;
                case 'E': IssueUnitListCommand("retreat_selected", "", L"retreat"); break;
                case 'Y': IssueUnitListCommand("emergency_deposit", "", L"emergency_deposit"); break;
                case 'U': IssueUnitListCommand("drop_cargo", "", L"drop_cargo"); break;
                case 'K':
                    if (shift_down) {
                        IssueUnitListCommand("intercept_route_threat", "", L"intercept_route_threat");
                    } else {
                        IssueUnitListCommand("guard_threatened_route", "", L"guard_threatened_route");
                    }
                    break;
                case 'V': {
                    if (!g_app.selected_units.empty()) {
                        const UnitView* unit = FindUnitById(g_app.selected_units.front());
                        IssueHold(unit != nullptr ? unit->position : dbd::Vec3 {});
                    }
                    break;
                }
                case 'X':
                    if (shift_down) {
                        IssueUnitListCommand("clear_queue", "", L"clear_queue");
                    } else {
                        IssueUnitListCommand("stop", "", L"stop");
                    }
                    break;
                case 'N': SelectNextContact(shift_down ? -1 : 1); break;
                case 'J': IssueInvestigateContact(); break;
                case VK_ADD:
                    g_app.pending_flatten_radius = std::min(24.0f, g_app.pending_flatten_radius + 1.0f);
                    g_app.status_line = L"Flatten radius " + std::to_wstring(static_cast<int>(g_app.pending_flatten_radius));
                    break;
                case VK_SUBTRACT:
                    g_app.pending_flatten_radius = std::max(3.0f, g_app.pending_flatten_radius - 1.0f);
                    g_app.status_line = L"Flatten radius " + std::to_wstring(static_cast<int>(g_app.pending_flatten_radius));
                    break;
                case VK_ESCAPE:
                    g_app.command_mode = CommandMode::Move;
                    g_app.queue_next_order = false;
                    g_app.pending_patrol_start_set = false;
                    g_app.pending_haul_source_set = false;
                    break;
                case VK_HOME: {
                    float sum_x = 0.0f;
                    float sum_z = 0.0f;
                    int count = 0;
                    for (const auto& unit : g_app.snapshot.units) {
                        if (!IsCommandableUnit(unit)) {
                            continue;
                        }
                        sum_x += unit.position.x;
                        sum_z += unit.position.z;
                        ++count;
                    }
                    if (count > 0) {
                        g_app.camera.center_x = sum_x / static_cast<float>(count);
                        g_app.camera.center_z = sum_z / static_cast<float>(count);
                        g_app.camera.distance = 72.0f;
                        g_app.status_line = L"Camera recentered on your people.";
                    }
                    break;
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const int delta = GET_WHEEL_DELTA_WPARAM(w_param);
            g_app.camera.distance = std::clamp(g_app.camera.distance - static_cast<float>(delta) * 0.08f, 38.0f, 180.0f);
            g_app.camera_moved_by_user = true;
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

fs::path RepoRootFromMacro() {
#ifdef DBD_REBOOT_ROOT
    return fs::path(DBD_REBOOT_ROOT);
#else
    return fs::current_path();
#endif
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_app.repo_root = RepoRootFromMacro();
    g_app.runtime_dir = g_app.repo_root / "runtime-save";

    WNDCLASSW wc {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"DBDRebootClient3DWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"DBD Reboot Client 3D",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1360,
        820,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd) {
        return 1;
    }
    g_app.hwnd = hwnd;
    if (!InitializeD3D(hwnd)) {
        MessageBoxW(hwnd, L"Failed to initialize Direct3D11.", L"DBD Client 3D", MB_ICONERROR);
        return 2;
    }
    ShowWindow(hwnd, show_command);

    MSG msg {};
    auto last_time = std::chrono::steady_clock::now();
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;
        UpdateCameraFromKeys(dt);
        UpdateRenderInterpolation(dt);
        Render();
    }
    return 0;
}
