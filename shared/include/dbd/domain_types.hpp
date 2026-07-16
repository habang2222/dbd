#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dbd {

using Id = std::uint64_t;

enum class UnitOrderType : std::uint8_t {
    Idle,
    Move,
    Harvest,
    HaulToStorage,
    Escort,
    AttackTarget,
    FlattenSite,
    BuildSite,
    Scout,
    Retreat
};

enum class StructureType : std::uint8_t {
    StorageDepot,
    Extractor,
    DefenseNode
};

enum class BuildTimeClass : std::uint8_t {
    SmallFast,
    MediumStandard,
    LargeSlow
};

enum class RegionRiskBand : std::uint8_t {
    Low,
    Medium,
    High
};

enum class AuthorityScopeType : std::uint8_t {
    Unit,
    Squad,
    Structure,
    Region,
    LineageWide
};

enum class AuthorityPermission : std::uint32_t {
    None = 0,
    IssueMove = 1 << 0,
    IssueGather = 1 << 1,
    IssueBuild = 1 << 2,
    IssueAttack = 1 << 3,
    AccessStorage = 1 << 4,
    WithdrawResource = 1 << 5,
    DepositResource = 1 << 6,
    ManageInsurance = 1 << 7,
    AssignMembers = 1 << 8
};

enum class JobKind : std::uint8_t {
    None,
    Harvest,
    Haul,
    Scout,
    Escort,
    Flatten,
    Build,
    Attack,
    Retreat
};

enum class CombatRangeBand : std::uint8_t {
    Short,
    Mid,
    Long
};

enum class AttackTargetKind : std::uint8_t {
    Unit,
    ConstructionSite,
    Structure
};

enum class SquadStance : std::uint8_t {
    Aggressive,
    Defensive,
    Retreat
};

enum class AutomationTrigger : std::uint8_t {
    LowHealth,
    InventoryHeavy,
    InventoryFull,
    EnemySeen
};

enum class AutomationAction : std::uint8_t {
    Retreat,
    ReturnToStorage,
    HoldPosition,
    AttackNearestEnemy
};

enum class CargoCategory : std::uint8_t {
    Resource,
    Equipment
};

enum class ItemCategory : std::uint8_t {
    Resource,
    ConstructionMaterial,
    Tool,
    Equipment
};

enum class HaulRouteSourceKind : std::uint8_t {
    None,
    ResourceNode,
    DroppedCargo
};

enum class HaulRoutePhase : std::uint8_t {
    None,
    ToSource,
    Loading,
    ToStorage,
    Unloading,
    Interrupted
};

enum class ConstructionStage : std::uint8_t {
    Planned,
    WaitingForFlatten,
    UnderConstruction,
    Completed,
    Destroyed,
    Canceled
};

enum class FlattenJobStateKind : std::uint8_t {
    Planned,
    InProgress,
    Completed,
    Canceled
};

struct Vec3 {
    float x {};
    float y {};
    float z {};
};

struct KnownContactState {
    Id observing_player_id {};
    Id target_entity_id {};
    AttackTargetKind target_kind {AttackTargetKind::Unit};
    Vec3 position {};
    std::uint64_t last_seen_tick {};
    Id spotted_by_unit_id {};
    bool currently_visible {};
};

struct ChunkCoord {
    std::int32_t x {};
    std::int32_t z {};
};

struct CargoStack {
    Id resource_id {};
    std::uint32_t amount {};
    float value_basis {};
    float unit_weight {1.0f};
    CargoCategory category {CargoCategory::Resource};
};

struct ItemDefinition {
    Id item_id {};
    const char* display_name {};
    ItemCategory category {ItemCategory::Resource};
    float unit_weight {1.0f};
    float base_value {};
    bool stackable {true};
    std::uint32_t max_stack {999};
};

inline const std::vector<ItemDefinition>& GetDefaultItemDefinitions() {
    static const std::vector<ItemDefinition> definitions {
        {91'001, "Basic Wood", ItemCategory::ConstructionMaterial, 1.0f, 6.0f, true, 999},
        {91'002, "Stone Block", ItemCategory::ConstructionMaterial, 2.4f, 10.0f, true, 500},
        {91'003, "Iron Fitting", ItemCategory::ConstructionMaterial, 1.2f, 18.0f, true, 300},
        {91'004, "Repair Material", ItemCategory::ConstructionMaterial, 1.0f, 10.0f, true, 500},
        {92'001, "Field Shovel", ItemCategory::Tool, 5.0f, 65.0f, false, 1},
        {92'002, "Flattening Tool", ItemCategory::Tool, 8.0f, 120.0f, false, 1},
        {92'003, "Builder Kit", ItemCategory::Tool, 7.0f, 140.0f, false, 1},
        {92'004, "Survey Marker", ItemCategory::Tool, 0.3f, 12.0f, true, 50},
        {92'005, "Storage Crate", ItemCategory::Tool, 4.0f, 45.0f, true, 20},
        {92'006, "Field Hammer", ItemCategory::Tool, 3.0f, 55.0f, false, 1}
    };
    return definitions;
}

inline const ItemDefinition* FindItemDefinition(Id item_id) {
    const auto& definitions = GetDefaultItemDefinitions();
    for (const auto& definition : definitions) {
        if (definition.item_id == item_id) {
            return &definition;
        }
    }
    return nullptr;
}

struct AutomationRule {
    AutomationTrigger trigger {AutomationTrigger::LowHealth};
    AutomationAction action {AutomationAction::Retreat};
    float threshold {0.0f};
    bool enabled {true};
};

struct UnitSkills {
    std::uint16_t strength_level {1};
    std::uint16_t harvest_level {1};
    std::uint16_t construction_level {1};
    std::uint16_t haul_level {1};
    std::uint16_t combat_level {1};
    std::uint16_t survival_level {1};
};

struct UnitSkillProgress {
    float strength_xp {};
    float harvest_xp {};
    float construction_xp {};
    float haul_xp {};
    float combat_xp {};
    float survival_xp {};
};

struct UnitLineage {
    Id lineage_id {};
    Id parent_a {};
    Id parent_b {};
    float combat_trait {1.0f};
    float labor_trait {1.0f};
    float survival_trait {1.0f};
    float fertility_trait {1.0f};
};

struct LineageState {
    Id lineage_id {};
    Id parent_lineage_id {};
    Id founder_unit_id {};
    Id controlling_player_id {};
    std::string display_name {};
    std::uint32_t member_count {};
    float military_value {};
    float economic_value {};
};

struct CommandAuthorityState {
    Id authority_id {};
    Id grantor_player_id {};
    Id grantee_player_id {};
    AuthorityScopeType scope_type {AuthorityScopeType::Unit};
    Id scope_target_id {};
    std::uint32_t permission_mask {static_cast<std::uint32_t>(AuthorityPermission::None)};
    bool revocable {true};
    std::uint64_t expires_at_utc_ms {};
};

struct WorkAssignmentState {
    JobKind kind {JobKind::None};
    Id target_entity_id {};
    AttackTargetKind target_kind {AttackTargetKind::Unit};
    Vec3 target_position {};
    Vec3 secondary_position {};
    float radius {};
    bool patrol_route {};
    bool patrol_to_secondary {};
    bool delegated {false};
    bool route_active {};
    HaulRouteSourceKind route_source_kind {HaulRouteSourceKind::None};
    HaulRoutePhase route_phase {HaulRoutePhase::None};
    Id route_source_id {};
    Id route_storage_id {};
};

struct QueuedOrderState {
    UnitOrderType order_type {UnitOrderType::Idle};
    WorkAssignmentState assignment {};
    Vec3 move_target {};
};

struct UnitState {
    Id unit_id {};
    Id owner_player_id {};
    Id controller_player_id {};
    Id squad_id {};
    UnitLineage lineage {};
    Vec3 position {};
    Vec3 move_target {};
    UnitOrderType current_order {UnitOrderType::Idle};
    WorkAssignmentState assignment {};
    UnitSkills skills {};
    UnitSkillProgress skill_progress {};
    CombatRangeBand combat_band {CombatRangeBand::Mid};
    float health {100.0f};
    float max_health {100.0f};
    float stamina {100.0f};
    float max_stamina {100.0f};
    float upkeep_cost {1.0f};
    float tax_weight {1.0f};
    float food_demand {1.0f};
    float carry_capacity {40.0f};
    float base_move_speed {4.0f};
    float overburden_threshold {1.0f};
    bool alive {true};
    bool permanently_dead {false};
    std::vector<CargoStack> cargo {};
    std::vector<AutomationRule> automation_rules {};
    std::vector<QueuedOrderState> queued_orders {};
    std::string tactical_state {};
    std::string queue_interrupt_reason {};
    std::string route_threat_level {"safe"};
    Id route_threat_enemy_id {};
};

struct SquadState {
    Id squad_id {};
    Id owner_player_id {};
    Id commander_player_id {};
    Id leader_unit_id {};
    std::string name {};
    SquadStance stance {SquadStance::Defensive};
    std::vector<Id> unit_ids {};
    std::vector<AutomationRule> automation_rules {};
};

struct ResourceNodeState {
    Id resource_node_id {};
    Vec3 position {};
    ChunkCoord chunk {};
    RegionRiskBand risk_band {RegionRiskBand::Low};
    float richness {1.0f};
    float extraction_rate {1.0f};
    std::uint32_t remaining_amount {};
    std::uint32_t max_amount {};
};

struct TerrainSample {
    float average_slope {};
    float roughness {};
    float flatten_work_estimate {};
    bool already_buildable {};
};

struct FlattenJobState {
    Id flatten_job_id {};
    Vec3 center {};
    float radius {};
    float target_grade {};
    ChunkCoord chunk {};
    float required_labor {};
    float accumulated_labor {};
    float initial_slope {};
    float current_slope {};
    FlattenJobStateKind state {FlattenJobStateKind::Planned};
    std::vector<Id> assigned_unit_ids {};
};

struct ConstructionSiteState {
    Id construction_site_id {};
    Id owner_player_id {};
    StructureType structure_type {StructureType::StorageDepot};
    BuildTimeClass time_class {BuildTimeClass::MediumStandard};
    Vec3 position {};
    ChunkCoord chunk {};
    float footprint_radius {};
    float allowed_slope {};
    float required_labor {};
    float accumulated_labor {};
    Id flatten_job_id {};
    bool flattening_required {false};
    ConstructionStage stage {ConstructionStage::Planned};
    float health {200.0f};
    float max_health {200.0f};
    float completion_health_ratio {};
    std::vector<Id> assigned_unit_ids {};
};

struct StructureState {
    Id structure_id {};
    Id owner_player_id {};
    StructureType structure_type {StructureType::StorageDepot};
    BuildTimeClass time_class {BuildTimeClass::MediumStandard};
    Vec3 position {};
    ChunkCoord chunk {};
    float footprint_radius {};
    float allowed_slope {};
    float health {500.0f};
    float max_health {500.0f};
    bool insurable {true};
};

struct StorageSiteState {
    Id storage_site_id {};
    Id structure_id {};
    Id owner_player_id {};
    Vec3 position {};
    ChunkCoord chunk {};
    std::vector<CargoStack> stored_resources {};
};

struct DroppedCargoState {
    Id dropped_cargo_id {};
    Vec3 position {};
    ChunkCoord chunk {};
    Id source_unit_id {};
    std::vector<CargoStack> cargo {};
    std::uint64_t spawned_at_utc_ms {};
};

struct TerrainFlattenStamp {
    Vec3 center {};
    float radius {};
    float applied_grade {};
};

struct CorpseState {
    Id corpse_id {};
    Id former_unit_id {};
    Vec3 position {};
    ChunkCoord chunk {};
    std::uint64_t time_of_death_utc_ms {};
};

struct InsurancePolicyState {
    Id policy_id {};
    Id insured_entity_id {};
    float insured_value {};
    float premium_rate {};
    float payout_rate {};
    std::uint64_t activation_time_utc_ms {};
    bool active {false};
};

struct RegionRiskProfileState {
    Id region_id {};
    RegionRiskBand band {RegionRiskBand::Low};
    float resource_density {1.0f};
    float resource_value_multiplier {1.0f};
    float travel_difficulty {1.0f};
    float flattening_cost_multiplier {1.0f};
    float defense_score {1.0f};
    float survival_score {1.0f};
};

struct ChunkState {
    ChunkCoord coord {};
    bool opened_today {false};
    bool loaded {false};
    std::uint64_t last_opened_utc_ms {};
    std::vector<TerrainFlattenStamp> flatten_stamps {};
};

struct PlayerState {
    Id player_id {};
    std::string display_name {};
    std::vector<Id> unit_ids {};
    std::vector<Id> squad_ids {};
    std::vector<Id> structure_ids {};
    float credits {};
    float tax_load {};
    float upkeep_load {};
    float credit_load {};
    float complexity_load {};
    float food_load {};
    float food_shortage_ratio {};
    float upkeep_shortage_ratio {};
    float shortage_ratio {};
    float efficiency {};
    float pressure_ratio {};
};

}  // namespace dbd
