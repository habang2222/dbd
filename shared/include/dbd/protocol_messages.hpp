#pragma once

#include "dbd/domain_types.hpp"

#include <string>
#include <variant>
#include <vector>

namespace dbd {

enum class MessageType : std::uint16_t {
    Hello,
    Login,
    LoginResult,
    JoinWorld,
    WorldSnapshot,
    WorldEvent,
    UnitOrderRequest,
    GroupAssignRequest,
    AuthorityTransferRequest,
    HarvestRequest,
    HaulRequest,
    StartFlattenJob,
    AssignUnitsToFlattenJob,
    FlattenJobProgress,
    FlattenJobCompleted,
    StartConstruction,
    AssignUnitsToConstruction,
    ConstructionProgress,
    ConstructionCompleted,
    ConstructionDamaged,
    ConstructionDestroyed,
    AttackRequest,
    InsuranceQuoteRequest,
    InsuranceBindRequest,
    OffspringRequest,
    UnitSkillProgressed,
    UnitDiedPermanent,
    DroppedCargoSpawned,
    DroppedCargoDestroyed,
    EventNotice,
    ErrorNotice
};

struct HelloMessage {
    std::string client_name {};
    std::uint32_t protocol_version {1};
};

struct LoginMessage {
    std::uint32_t request_id {};
    std::string auth_token {};
    std::string desired_name {};
};

struct LoginResultMessage {
    std::uint32_t request_id {};
    bool ok {};
    Id player_id {};
    std::string session_token {};
    std::string error_code {};
};

struct JoinWorldMessage {
    std::uint32_t request_id {};
    std::string player_name {};
};

struct WorldSnapshotMessage {
    std::vector<PlayerState> players {};
    std::vector<CommandAuthorityState> authorities {};
    std::vector<LineageState> lineages {};
    std::vector<UnitState> units {};
    std::vector<SquadState> squads {};
    std::vector<ResourceNodeState> resource_nodes {};
    std::vector<FlattenJobState> flatten_jobs {};
    std::vector<ConstructionSiteState> construction_sites {};
    std::vector<StructureState> structures {};
    std::vector<StorageSiteState> storage_sites {};
    std::vector<DroppedCargoState> dropped_cargo {};
    std::vector<CorpseState> corpses {};
    std::vector<RegionRiskProfileState> regions {};
    std::vector<ChunkState> chunks {};
};

struct UnitOrderRequestMessage {
    Id controller_player_id {};
    std::vector<Id> unit_ids {};
    UnitOrderType order_type {UnitOrderType::Idle};
    Vec3 target_position {};
    Id target_entity_id {};
};

struct GroupAssignRequestMessage {
    Id owner_player_id {};
    Id squad_id {};
    std::vector<Id> unit_ids {};
};

struct AuthorityTransferRequestMessage {
    Id owner_player_id {};
    Id target_player_id {};
    std::vector<Id> unit_ids {};
    std::vector<Id> squad_ids {};
    AuthorityScopeType scope_type {AuthorityScopeType::Unit};
    std::uint32_t permission_mask {static_cast<std::uint32_t>(AuthorityPermission::None)};
    bool revocable {true};
    std::uint64_t expires_at_utc_ms {};
};

struct HarvestRequestMessage {
    Id unit_id {};
    Id resource_node_id {};
};

struct HaulRequestMessage {
    Id unit_id {};
    Id storage_site_id {};
};

struct StartFlattenJobMessage {
    Id player_id {};
    Vec3 center {};
    float radius {};
    float target_grade {};
};

struct AssignUnitsToFlattenJobMessage {
    Id player_id {};
    Id flatten_job_id {};
    std::vector<Id> unit_ids {};
};

struct FlattenJobProgressMessage {
    Id flatten_job_id {};
    float accumulated_labor {};
    float required_labor {};
    FlattenJobStateKind state {FlattenJobStateKind::Planned};
};

struct FlattenJobCompletedMessage {
    Id flatten_job_id {};
    Vec3 center {};
    float radius {};
};

struct StartConstructionMessage {
    Id player_id {};
    StructureType structure_type {StructureType::StorageDepot};
    Vec3 position {};
    float footprint_radius {};
};

struct AssignUnitsToConstructionMessage {
    Id player_id {};
    Id construction_site_id {};
    std::vector<Id> unit_ids {};
};

struct ConstructionProgressMessage {
    Id construction_site_id {};
    float accumulated_labor {};
    float required_labor {};
    ConstructionStage stage {ConstructionStage::Planned};
};

struct ConstructionCompletedMessage {
    Id construction_site_id {};
    Id structure_id {};
};

struct ConstructionDamagedMessage {
    Id construction_site_id {};
    float health {};
    float max_health {};
};

struct ConstructionDestroyedMessage {
    Id construction_site_id {};
};

struct AttackRequestMessage {
    Id controller_player_id {};
    std::vector<Id> attacker_unit_ids {};
    AttackTargetKind target_kind {AttackTargetKind::Unit};
    Id target_entity_id {};
};

struct InsuranceQuoteRequestMessage {
    Id player_id {};
    Id insured_entity_id {};
    float declared_value {};
    float requested_premium_rate {};
};

struct InsuranceBindRequestMessage {
    Id player_id {};
    Id insured_entity_id {};
    float insured_value {};
    float premium_rate {};
};

struct OffspringRequestMessage {
    Id player_id {};
    Id parent_a_unit_id {};
    Id parent_b_unit_id {};
};

struct UnitSkillProgressedMessage {
    Id unit_id {};
    UnitSkills skills {};
    UnitSkillProgress progress {};
};

struct UnitDiedPermanentMessage {
    Id unit_id {};
    Vec3 death_position {};
    float insurance_payout {};
};

struct DroppedCargoSpawnedMessage {
    Id dropped_cargo_id {};
    Id source_unit_id {};
    Vec3 position {};
    std::vector<CargoStack> cargo {};
};

struct DroppedCargoDestroyedMessage {
    Id dropped_cargo_id {};
};

struct EventNoticeMessage {
    std::uint64_t tick {};
    std::string code {};
    std::string description {};
};

struct ErrorNoticeMessage {
    std::string code {};
    std::string description {};
};

using ProtocolMessage = std::variant<
    HelloMessage,
    LoginMessage,
    LoginResultMessage,
    JoinWorldMessage,
    WorldSnapshotMessage,
    UnitOrderRequestMessage,
    GroupAssignRequestMessage,
    AuthorityTransferRequestMessage,
    HarvestRequestMessage,
    HaulRequestMessage,
    StartFlattenJobMessage,
    AssignUnitsToFlattenJobMessage,
    FlattenJobProgressMessage,
    FlattenJobCompletedMessage,
    StartConstructionMessage,
    AssignUnitsToConstructionMessage,
    ConstructionProgressMessage,
    ConstructionCompletedMessage,
    ConstructionDamagedMessage,
    ConstructionDestroyedMessage,
    AttackRequestMessage,
    InsuranceQuoteRequestMessage,
    InsuranceBindRequestMessage,
    OffspringRequestMessage,
    UnitSkillProgressedMessage,
    UnitDiedPermanentMessage,
    DroppedCargoSpawnedMessage,
    DroppedCargoDestroyedMessage,
    EventNoticeMessage,
    ErrorNoticeMessage>;

}  // namespace dbd
