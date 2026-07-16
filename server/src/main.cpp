#include "dbd_server/command_spool.hpp"
#include "dbd_server/server_simulation.hpp"
#include "dbd_server/world_bootstrap.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct RegressionStep {
    std::string name;
    bool passed;
    std::string message;
};

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

void RecordStep(std::vector<RegressionStep>& steps, const std::string& name, bool passed, const std::string& message) {
    steps.push_back({name, passed, message});
}

float CargoWeight(const std::vector<dbd::CargoStack>& cargo) {
    float total = 0.0f;
    for (const auto& stack : cargo) {
        total += static_cast<float>(stack.amount) * stack.unit_weight;
    }
    return total;
}

float Distance(const dbd::Vec3& a, const dbd::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

bool WriteRegressionReportJson(
    const std::filesystem::path& output_path,
    bool overall_pass,
    std::uint64_t tick,
    const std::filesystem::path& world_save_root,
    const std::filesystem::path& command_spool_path,
    const std::filesystem::path& snapshot_path,
    const std::vector<RegressionStep>& steps) {
    std::ofstream out(output_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"overallPass\": " << (overall_pass ? "true" : "false") << ",\n";
    out << "  \"tick\": " << tick << ",\n";
    out << "  \"worldSaveRoot\": \"" << JsonEscape(world_save_root.string()) << "\",\n";
    out << "  \"savePath\": \"" << JsonEscape(world_save_root.string()) << "\",\n";
    out << "  \"commandSpoolPath\": \"" << JsonEscape(command_spool_path.string()) << "\",\n";
    out << "  \"snapshotPath\": \"" << JsonEscape(snapshot_path.string()) << "\",\n";
    out << "  \"steps\": [\n";
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        out << "    {\"name\": \"" << JsonEscape(step.name) << "\", \"passed\": "
            << (step.passed ? "true" : "false") << ", \"message\": \"" << JsonEscape(step.message) << "\"}";
        if (i + 1 != steps.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

}  // namespace

int main() {
    std::cout << "DBD authoritative server scaffold booting..." << std::endl;
    dbd_server::WorldState world;
    dbd_server::InitializeExperimentalChunkGrid(world);
    bool regression_ok = true;
    std::vector<RegressionStep> regression_steps;
    const auto bootstrap = dbd_server::BootstrapSinglePlayerWarSlice(world, "FoundingPlayer");
    const auto raider_bootstrap = dbd_server::BootstrapSinglePlayerWarSlice(world, "RaiderPlayer");
    RecordStep(
        regression_steps,
        "bootstrap_spawn",
        bootstrap.starter_unit_ids.size() == 4 && raider_bootstrap.starter_unit_ids.size() == 4,
        "Both players should spawn with four starter units.");

    std::cout << "Spawned player " << bootstrap.player_id
              << " with " << bootstrap.starter_unit_ids.size()
              << " flexible starter units." << std::endl;
    std::cout << "Spawned second player " << raider_bootstrap.player_id
              << " for contested-site testing." << std::endl;

    {
        const auto& items = dbd::GetDefaultItemDefinitions();
        const bool item_definitions_ok =
            items.size() >= 10 &&
            dbd::FindItemDefinition(91'001) != nullptr &&
            dbd::FindItemDefinition(91'004) != nullptr &&
            dbd::FindItemDefinition(92'002) != nullptr &&
            dbd::FindItemDefinition(92'006) != nullptr;
        RecordStep(
            regression_steps,
            "item_definition_table",
            item_definitions_ok,
            item_definitions_ok
                ? "Default construction/tool item definitions are registered."
                : "Default construction/tool item definitions are missing.");
        regression_ok = regression_ok && item_definitions_ok;
    }

    if (world.storage_sites.find(bootstrap.depot_storage_id) != world.storage_sites.end()) {
        auto& storage = world.storage_sites.at(bootstrap.depot_storage_id);
        storage.stored_resources.push_back(dbd::CargoStack {91'001, 40, 240.0f, 1.0f, dbd::CargoCategory::Resource});
        storage.stored_resources.push_back(dbd::CargoStack {91'002, 12, 120.0f, 2.4f, dbd::CargoCategory::Resource});
        storage.stored_resources.push_back(dbd::CargoStack {91'003, 12, 216.0f, 1.2f, dbd::CargoCategory::Resource});
        const auto craft_shovel = dbd_server::CraftItem(world, bootstrap.player_id, 92'001, 1);
        const auto craft_marker = dbd_server::CraftItem(world, bootstrap.player_id, 92'004, 1);
        const auto craft_crate = dbd_server::CraftItem(world, bootstrap.player_id, 92'005, 1);
        const auto install_marker = dbd_server::InstallItem(world, bootstrap.player_id, 92'004, dbd::Vec3 {54.0f, 0.0f, 8.0f});
        const auto use_shovel = dbd_server::UseItem(world, bootstrap.player_id, {bootstrap.starter_unit_ids.front()}, 92'001);
        const bool craft_install_use_ok =
            craft_shovel.ok &&
            craft_marker.ok &&
            craft_crate.ok &&
            install_marker.ok &&
            use_shovel.ok &&
            world.units.at(bootstrap.starter_unit_ids.front()).tactical_state.find("Using Field Shovel") != std::string::npos;
        RecordStep(
            regression_steps,
            "simple_craft_install_use",
            craft_install_use_ok,
            craft_install_use_ok ? "Crafted tools, installed a survey marker, and used a shovel." : craft_marker.message);
        regression_ok = regression_ok && craft_install_use_ok;
    }

    if (bootstrap.resource_node_ids.size() >= 3) {
        const auto& starter_node = world.resource_nodes.at(bootstrap.resource_node_ids[0]);
        const auto& mid_node = world.resource_nodes.at(bootstrap.resource_node_ids[1]);
        const auto& frontier_node = world.resource_nodes.at(bootstrap.resource_node_ids[2]);
        const float starter_score = starter_node.richness * starter_node.extraction_rate;
        const float mid_score = mid_node.richness * mid_node.extraction_rate;
        const float frontier_score = frontier_node.richness * frontier_node.extraction_rate;
        const bool resource_risk_reward =
            starter_node.risk_band == dbd::RegionRiskBand::Low &&
            mid_node.risk_band == dbd::RegionRiskBand::Medium &&
            frontier_node.risk_band == dbd::RegionRiskBand::High &&
            starter_score < mid_score &&
            mid_score < frontier_score;
        RecordStep(
            regression_steps,
            "resource_risk_reward_tiers",
            resource_risk_reward,
            resource_risk_reward
                ? "Starter, mid-field, and frontier resource nodes increase reward with risk."
                : "Resource risk/reward tiers are not ordered.");
        regression_ok = regression_ok && resource_risk_reward;
    }

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

    const std::vector<dbd::AutomationRule> veteran_rules {
        dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.35f, true},
        dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::AttackNearestEnemy, 0.0f, true}};
    const auto unit_automation_result = dbd_server::SetUnitAutomationRules(
        world,
        bootstrap.player_id,
        bootstrap.starter_unit_ids.front(),
        veteran_rules);
    RecordStep(
        regression_steps,
        "unit_automation_edit",
        unit_automation_result.ok &&
            world.units.at(bootstrap.starter_unit_ids.front()).automation_rules.size() == veteran_rules.size(),
        unit_automation_result.message);
    regression_ok = regression_ok && unit_automation_result.ok;

    const std::vector<dbd::AutomationRule> squad_rules {
        dbd::AutomationRule {dbd::AutomationTrigger::InventoryFull, dbd::AutomationAction::ReturnToStorage, 0.90f, true}};
    const auto squad_automation_result = dbd_server::SetSquadAutomationRules(
        world,
        bootstrap.player_id,
        bootstrap.squad_id,
        squad_rules);
    RecordStep(
        regression_steps,
        "squad_automation_edit",
        squad_automation_result.ok &&
            world.squads.at(bootstrap.squad_id).automation_rules.size() == squad_rules.size(),
        squad_automation_result.message);
    regression_ok = regression_ok && squad_automation_result.ok;

    if (bootstrap.starter_unit_ids.size() >= 4) {
        auto& guard = world.units.at(bootstrap.starter_unit_ids[2]);
        auto& protected_unit = world.units.at(bootstrap.starter_unit_ids[0]);
        protected_unit.position = {20.0f, 0.0f, 20.0f};
        protected_unit.move_target = protected_unit.position;
        guard.position = {4.0f, 0.0f, 4.0f};
        guard.move_target = guard.position;
        const auto guard_unit_result = dbd_server::IssueGuardUnitOrder(
            world,
            bootstrap.player_id,
            {guard.unit_id},
            protected_unit.unit_id);
        const auto guard_progress = dbd_server::ProgressGuardOrders(world, 1.0f);
        const auto guard_move = dbd_server::ProgressUnitMovement(world, 1.0f);
        const bool guard_moving_to_protect =
            world.units.at(guard.unit_id).current_order == dbd::UnitOrderType::Escort &&
            world.units.at(guard.unit_id).assignment.target_entity_id == protected_unit.unit_id &&
            world.units.at(guard.unit_id).assignment.target_position.x == protected_unit.position.x;
        RecordStep(
            regression_steps,
            "guard_unit_order",
            guard_unit_result.ok && guard_progress.ok && guard_move.ok && guard_moving_to_protect,
            guard_unit_result.ok ? "Guard unit moved toward protected unit." : guard_unit_result.message);
        regression_ok = regression_ok && guard_unit_result.ok && guard_progress.ok && guard_move.ok && guard_moving_to_protect;

        auto& holder = world.units.at(bootstrap.starter_unit_ids[3]);
        holder.position = {30.0f, 0.0f, 30.0f};
        holder.move_target = holder.position;
        const dbd::Vec3 hold_position {34.0f, 0.0f, 32.0f};
        const auto hold_result = dbd_server::IssueHoldPositionOrder(
            world,
            bootstrap.player_id,
            {holder.unit_id},
            hold_position);
        const auto hold_progress = dbd_server::ProgressGuardOrders(world, 1.0f);
        const bool hold_registered =
            world.units.at(holder.unit_id).current_order == dbd::UnitOrderType::Escort &&
            world.units.at(holder.unit_id).assignment.target_entity_id == 0;
        RecordStep(
            regression_steps,
            "hold_position_order",
            hold_result.ok && hold_progress.ok && hold_registered,
            hold_result.ok ? "Hold position registered as an escort/defense assignment." : hold_result.message);
        regression_ok = regression_ok && hold_result.ok && hold_progress.ok && hold_registered;

        const auto stop_result = dbd_server::IssueStopOrder(
            world,
            bootstrap.player_id,
            {guard.unit_id, holder.unit_id});
        const bool stopped =
            world.units.at(guard.unit_id).current_order == dbd::UnitOrderType::Idle &&
            world.units.at(holder.unit_id).current_order == dbd::UnitOrderType::Idle;
        RecordStep(
            regression_steps,
            "stop_order",
            stop_result.ok && stopped,
            stop_result.ok ? "Stop cleared guard/hold orders." : stop_result.message);
        regression_ok = regression_ok && stop_result.ok && stopped;

        const auto retreat_result = dbd_server::IssueRetreatOrder(
            world,
            bootstrap.player_id,
            {guard.unit_id});
        const bool retreat_registered =
            world.units.at(guard.unit_id).current_order == dbd::UnitOrderType::Retreat &&
            world.units.at(guard.unit_id).assignment.kind == dbd::JobKind::Retreat;
        RecordStep(
            regression_steps,
            "retreat_selected_order",
            retreat_result.ok && retreat_registered,
            retreat_result.ok ? "Manual retreat converted the selected unit to Retreat." : retreat_result.message);
        regression_ok = regression_ok && retreat_result.ok && retreat_registered;

        if (raider_bootstrap.starter_unit_ids.size() >= 2) {
            auto& focus_target = world.units.at(raider_bootstrap.starter_unit_ids[0]);
            auto& focus_attacker = world.units.at(holder.unit_id);
            focus_target.alive = true;
            focus_target.permanently_dead = false;
            focus_target.health = focus_target.max_health;
            focus_target.position = {focus_attacker.position.x + 3.0f, focus_attacker.position.y, focus_attacker.position.z};
            focus_attacker.position = {40.0f, 0.0f, 40.0f};
            focus_target.position = {43.0f, 0.0f, 40.0f};
            dbd_server::RefreshKnownContacts(world);
            const auto contact_it = world.known_contacts.find(bootstrap.player_id);
            const bool contact_visible =
                contact_it != world.known_contacts.end() &&
                contact_it->second.find(focus_target.unit_id) != contact_it->second.end() &&
                contact_it->second.at(focus_target.unit_id).currently_visible;
            RecordStep(
                regression_steps,
                "known_contact_visible",
                contact_visible,
                contact_visible ? "Enemy entered sight and created a visible contact." : "Enemy contact was not created.");
            regression_ok = regression_ok && contact_visible;

            focus_target.position = {90.0f, 0.0f, 40.0f};
            dbd_server::AdvanceWorldTime(world, 9000);
            dbd_server::RefreshKnownContacts(world);
            const auto stale_contact_it = world.known_contacts.find(bootstrap.player_id);
            const bool contact_stale =
                stale_contact_it != world.known_contacts.end() &&
                stale_contact_it->second.find(focus_target.unit_id) != stale_contact_it->second.end() &&
                !stale_contact_it->second.at(focus_target.unit_id).currently_visible;
            RecordStep(
                regression_steps,
                "known_contact_stale",
                contact_stale,
                contact_stale ? "Enemy left sight and remained as a stale contact." : "Stale contact was not retained.");
            regression_ok = regression_ok && contact_stale;

            const auto stale_focus_result = dbd_server::IssueFocusFireOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                dbd::AttackTargetKind::Unit,
                focus_target.unit_id);
            const bool stale_focus_search =
                stale_focus_result.ok &&
                world.units.at(focus_attacker.unit_id).current_order == dbd::UnitOrderType::Move &&
                world.units.at(focus_attacker.unit_id).assignment.kind == dbd::JobKind::Scout;
            RecordStep(
                regression_steps,
                "stale_contact_search",
                stale_focus_search,
                stale_focus_search ? "Stale focus target became last-seen search movement." : stale_focus_result.message);
            regression_ok = regression_ok && stale_focus_search;

            focus_attacker.current_order = dbd::UnitOrderType::Idle;
            focus_attacker.assignment = {};
            focus_attacker.position = {40.0f, 0.0f, 42.0f};
            const auto investigate_stale_result = dbd_server::IssueInvestigateContactOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                dbd::AttackTargetKind::Unit,
                focus_target.unit_id);
            const bool investigate_stale_move =
                investigate_stale_result.ok &&
                world.units.at(focus_attacker.unit_id).current_order == dbd::UnitOrderType::Move &&
                world.units.at(focus_attacker.unit_id).assignment.target_entity_id == focus_target.unit_id;
            RecordStep(
                regression_steps,
                "investigate_stale_contact",
                investigate_stale_move,
                investigate_stale_move ? "Investigate contact moved toward the stale last-seen position." : investigate_stale_result.message);
            regression_ok = regression_ok && investigate_stale_move;

            focus_attacker.current_order = dbd::UnitOrderType::Idle;
            focus_attacker.assignment = {};
            focus_attacker.position = {40.0f, 0.0f, 40.0f};
            focus_target.position = {43.0f, 0.0f, 40.0f};
            dbd_server::RefreshKnownContacts(world);
            const float focus_health_before = focus_target.health;
            const auto focus_result = dbd_server::IssueFocusFireOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                dbd::AttackTargetKind::Unit,
                focus_target.unit_id);
            dbd_server::ProgressCombat(world, 1.0f);
            const bool focus_damaged = world.units.at(focus_target.unit_id).health < focus_health_before;
            RecordStep(
                regression_steps,
                "focus_fire_order",
                focus_result.ok && focus_damaged,
                focus_result.ok && focus_damaged ? "Focus fire registered and dealt damage." : focus_result.message);
            regression_ok = regression_ok && focus_result.ok && focus_damaged;

            auto& moving_target = world.units.at(raider_bootstrap.starter_unit_ids[1]);
            moving_target.alive = true;
            moving_target.permanently_dead = false;
            moving_target.health = moving_target.max_health;
            moving_target.position = {80.0f, 0.0f, 80.0f};
            moving_target.move_target = {100.0f, 0.0f, 80.0f};
            focus_attacker.position = {70.0f, 0.0f, 70.0f};
            const auto intercept_result = dbd_server::IssueInterceptUnitOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                moving_target.unit_id);
            const auto& interceptor = world.units.at(focus_attacker.unit_id);
            const bool intercept_registered =
                interceptor.current_order == dbd::UnitOrderType::Move &&
                interceptor.assignment.kind == dbd::JobKind::Scout &&
                interceptor.assignment.target_entity_id == moving_target.unit_id &&
                interceptor.move_target.x > moving_target.position.x &&
                interceptor.move_target.x < moving_target.move_target.x;
            RecordStep(
                regression_steps,
                "intercept_unit_order",
                intercept_result.ok && intercept_registered,
                intercept_result.ok ? "Intercept computed a route ahead of the moving target." : intercept_result.message);
            regression_ok = regression_ok && intercept_result.ok && intercept_registered;

            const dbd::Vec3 scout_center {moving_target.position.x, moving_target.position.y, moving_target.position.z};
            focus_attacker.position = {scout_center.x - 20.0f, 0.0f, scout_center.z};
            const auto scout_result = dbd_server::IssueScoutAreaOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                scout_center,
                18.0f);
            for (int i = 0; i < 8; ++i) {
                dbd_server::ProgressUnitMovement(world, 1.0f);
                dbd_server::RefreshKnownContacts(world);
            }
            const auto scout_contact_it = world.known_contacts.find(bootstrap.player_id);
            const bool scout_contact =
                scout_contact_it != world.known_contacts.end() &&
                scout_contact_it->second.find(moving_target.unit_id) != scout_contact_it->second.end();
            RecordStep(
                regression_steps,
                "scout_area_contact",
                scout_result.ok && scout_contact,
                scout_result.ok && scout_contact ? "Scout area movement generated a contact." : scout_result.message);
            regression_ok = regression_ok && scout_result.ok && scout_contact;

            const dbd::Vec3 patrol_a {moving_target.position.x - 8.0f, 0.0f, moving_target.position.z};
            const dbd::Vec3 patrol_b {moving_target.position.x + 8.0f, 0.0f, moving_target.position.z};
            const auto patrol_result = dbd_server::IssuePatrolRouteOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                patrol_a,
                patrol_b);
            const auto patrol_move_1 = dbd_server::ProgressUnitMovement(world, 1.0f);
            const bool patrol_registered =
                patrol_result.ok &&
                world.units.at(focus_attacker.unit_id).current_order == dbd::UnitOrderType::Scout &&
                world.units.at(focus_attacker.unit_id).assignment.patrol_route;
            RecordStep(
                regression_steps,
                "patrol_route_order",
                patrol_registered && patrol_move_1.ok,
                patrol_registered ? "Patrol route registered as scout movement." : patrol_result.message);
            regression_ok = regression_ok && patrol_registered && patrol_move_1.ok;

            focus_target.position = {220.0f, 0.0f, 220.0f};
            dbd_server::AdvanceWorldTime(world, 130000);
            dbd_server::RefreshKnownContacts(world);
            const auto investigate_expired_result = dbd_server::IssueInvestigateContactOrder(
                world,
                bootstrap.player_id,
                {focus_attacker.unit_id},
                dbd::AttackTargetKind::Unit,
                focus_target.unit_id);
            RecordStep(
                regression_steps,
                "investigate_expired_contact",
                !investigate_expired_result.ok,
                !investigate_expired_result.ok ? "Expired contact was rejected." : "Expired contact unexpectedly accepted.");
            regression_ok = regression_ok && !investigate_expired_result.ok;
        }
    }

    if (world.storage_sites.find(bootstrap.depot_storage_id) != world.storage_sites.end()) {
        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.push_back(
            dbd::CargoStack {80'001, 40, 200.0f, 1.0f, dbd::CargoCategory::Resource});
    }

    if (!bootstrap.starter_unit_ids.empty()) {
        auto& mover = world.units.at(bootstrap.starter_unit_ids.front());
        mover.current_order = dbd::UnitOrderType::Idle;
        mover.assignment = {};
        const dbd::Vec3 start_position = mover.position;
        const dbd::Vec3 move_target {start_position.x + 6.0f, start_position.y, start_position.z + 3.0f};
        const auto move_order = dbd_server::IssueMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids.front()},
            move_target);
        const auto move_tick = dbd_server::ProgressUnitMovement(world, 1.0f);
        const bool moved = world.units.at(bootstrap.starter_unit_ids.front()).position.x != start_position.x ||
                           world.units.at(bootstrap.starter_unit_ids.front()).position.z != start_position.z;
        RecordStep(
            regression_steps,
            "move_order",
            move_order.ok && move_tick.ok && moved,
            move_order.ok && moved ? "Move command changed unit position." : move_order.message);
        regression_ok = regression_ok && move_order.ok && move_tick.ok && moved;
    }

    if (bootstrap.starter_unit_ids.size() >= 2) {
        const dbd::Vec3 formation_target {38.0f, 0.0f, 18.0f};
        const auto formation_result = dbd_server::IssueFormationMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[0], bootstrap.starter_unit_ids[1]},
            formation_target);
        const auto& formation_a = world.units.at(bootstrap.starter_unit_ids[0]);
        const auto& formation_b = world.units.at(bootstrap.starter_unit_ids[1]);
        const bool formation_offsets =
            std::abs(formation_a.move_target.x - formation_b.move_target.x) > 0.1f ||
            std::abs(formation_a.move_target.z - formation_b.move_target.z) > 0.1f;
        RecordStep(
            regression_steps,
            "formation_move_offsets",
            formation_result.ok && formation_offsets,
            formation_offsets ? "Formation move assigned distinct offset targets." : formation_result.message);
        regression_ok = regression_ok && formation_result.ok && formation_offsets;

        const dbd::Vec3 queued_target {46.0f, 0.0f, 22.0f};
        const auto queue_result = dbd_server::QueueMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[0]},
            queued_target);
        auto& queued_unit = world.units.at(bootstrap.starter_unit_ids[0]);
        const bool queue_visible = queue_result.ok && queued_unit.queued_orders.size() == 1;
        if (queue_visible) {
            queued_unit.position = queued_unit.move_target;
            const auto queue_tick = dbd_server::ProgressUnitMovement(world, 0.1f);
            const bool queue_started =
                queue_tick.ok &&
                queued_unit.current_order == dbd::UnitOrderType::Move &&
                std::abs(queued_unit.move_target.x - queued_target.x) < 0.1f &&
                queued_unit.queued_orders.empty();
            RecordStep(
                regression_steps,
                "queued_move_execution",
                queue_started,
                queue_started ? "Queued move started after the active move completed." : "Queued move did not start after arrival.");
            regression_ok = regression_ok && queue_started;
        } else {
            RecordStep(regression_steps, "queued_move_execution", false, queue_result.message);
            regression_ok = false;
        }

        const auto queue_stop_result = dbd_server::QueueMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[1]},
            {52.0f, 0.0f, 24.0f});
        const auto stop_queue_result = dbd_server::IssueStopOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[1]});
        const bool stop_cleared_queue =
            queue_stop_result.ok &&
            stop_queue_result.ok &&
            world.units.at(bootstrap.starter_unit_ids[1]).queued_orders.empty();
        RecordStep(
            regression_steps,
            "stop_clears_queued_orders",
            stop_cleared_queue,
            stop_cleared_queue ? "Stop cleared queued orders." : stop_queue_result.message);
        regression_ok = regression_ok && stop_cleared_queue;

        const auto queue_preserve_result = dbd_server::QueueMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[1]},
            {56.0f, 0.0f, 26.0f});
        const auto active_order_before_clear = world.units.at(bootstrap.starter_unit_ids[1]).current_order;
        const auto clear_queue_result = dbd_server::ClearQueuedOrders(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[1]});
        const bool clear_queue_preserved_active_order =
            queue_preserve_result.ok &&
            clear_queue_result.ok &&
            world.units.at(bootstrap.starter_unit_ids[1]).queued_orders.empty() &&
            world.units.at(bootstrap.starter_unit_ids[1]).current_order == active_order_before_clear;
        RecordStep(
            regression_steps,
            "clear_queue_preserves_active_order",
            clear_queue_preserved_active_order,
            clear_queue_preserved_active_order ? "Clear queue removed reservations without changing active order." : clear_queue_result.message);
        regression_ok = regression_ok && clear_queue_preserved_active_order;

        if (!raider_bootstrap.starter_unit_ids.empty()) {
            auto& gather_unit = world.units.at(bootstrap.starter_unit_ids[1]);
            auto& raider_contact = world.units.at(raider_bootstrap.starter_unit_ids[0]);
            gather_unit.position = {20.0f, 0.0f, 20.0f};
            gather_unit.move_target = {42.0f, 0.0f, 20.0f};
            gather_unit.current_order = dbd::UnitOrderType::Move;
            gather_unit.automation_rules = {
                dbd::AutomationRule {dbd::AutomationTrigger::InventoryFull, dbd::AutomationAction::ReturnToStorage, 0.90f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::Retreat, 0.0f, true}};
            gather_unit.queued_orders.clear();
            dbd_server::QueueMoveOrder(world, bootstrap.player_id, {gather_unit.unit_id}, {55.0f, 0.0f, 20.0f});
            raider_contact.position = {gather_unit.position.x + 3.0f, 0.0f, gather_unit.position.z};
            const auto gather_auto = dbd_server::EvaluateAutomation(world, 1.0f);
            const bool gather_retreats =
                gather_auto.ok &&
                gather_unit.current_order == dbd::UnitOrderType::Retreat &&
                gather_unit.queued_orders.empty() &&
                gather_unit.queue_interrupt_reason.find("enemy") != std::string::npos;
            RecordStep(
                regression_steps,
                "queued_gather_enemy_retreats",
                gather_retreats,
                gather_retreats ? "Gather preset interrupted queued movement and retreated on contact." : gather_auto.message);
            regression_ok = regression_ok && gather_retreats;

            auto& escort_unit = world.units.at(bootstrap.starter_unit_ids[0]);
            escort_unit.position = {24.0f, 0.0f, 24.0f};
            escort_unit.move_target = {46.0f, 0.0f, 24.0f};
            escort_unit.current_order = dbd::UnitOrderType::Move;
            escort_unit.automation_rules = {
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::AttackNearestEnemy, 0.0f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.30f, true}};
            escort_unit.queued_orders.clear();
            dbd_server::QueueMoveOrder(world, bootstrap.player_id, {escort_unit.unit_id}, {58.0f, 0.0f, 24.0f});
            raider_contact.position = {escort_unit.position.x + 3.0f, 0.0f, escort_unit.position.z};
            const auto escort_auto = dbd_server::EvaluateAutomation(world, 1.0f);
            const bool escort_attacks =
                escort_auto.ok &&
                escort_unit.current_order == dbd::UnitOrderType::AttackTarget &&
                escort_unit.queued_orders.empty() &&
                escort_unit.queue_interrupt_reason.find("enemy") != std::string::npos;
            RecordStep(
                regression_steps,
                "queued_escort_enemy_attacks",
                escort_attacks,
                escort_attacks ? "Escort preset interrupted queued movement and engaged contact." : escort_auto.message);
            regression_ok = regression_ok && escort_attacks;

            auto& hold_unit = world.units.at(bootstrap.starter_unit_ids[1]);
            hold_unit.position = {28.0f, 0.0f, 28.0f};
            hold_unit.move_target = {48.0f, 0.0f, 28.0f};
            hold_unit.current_order = dbd::UnitOrderType::Move;
            hold_unit.automation_rules = {
                dbd::AutomationRule {dbd::AutomationTrigger::EnemySeen, dbd::AutomationAction::HoldPosition, 0.0f, true},
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.25f, true}};
            hold_unit.queued_orders.clear();
            dbd_server::QueueMoveOrder(world, bootstrap.player_id, {hold_unit.unit_id}, {60.0f, 0.0f, 28.0f});
            raider_contact.position = {hold_unit.position.x + 3.0f, 0.0f, hold_unit.position.z};
            const auto hold_auto = dbd_server::EvaluateAutomation(world, 1.0f);
            const bool hold_keeps_position =
                hold_auto.ok &&
                hold_unit.current_order == dbd::UnitOrderType::Escort &&
                hold_unit.queued_orders.empty() &&
                hold_unit.queue_interrupt_reason.find("held") != std::string::npos;
            RecordStep(
                regression_steps,
                "queued_hold_enemy_holds",
                hold_keeps_position,
                hold_keeps_position ? "Hold preset interrupted queued movement and anchored in place." : hold_auto.message);
            regression_ok = regression_ok && hold_keeps_position;

            hold_unit.health = hold_unit.max_health * 0.15f;
            hold_unit.automation_rules = {
                dbd::AutomationRule {dbd::AutomationTrigger::LowHealth, dbd::AutomationAction::Retreat, 0.25f, true}};
            dbd_server::QueueMoveOrder(world, bootstrap.player_id, {hold_unit.unit_id}, {62.0f, 0.0f, 28.0f});
            const auto low_health_auto = dbd_server::EvaluateAutomation(world, 1.0f);
            const bool low_health_overrides_queue =
                low_health_auto.ok &&
                hold_unit.current_order == dbd::UnitOrderType::Retreat &&
                hold_unit.queued_orders.empty() &&
                hold_unit.queue_interrupt_reason == "Retreat override";
            RecordStep(
                regression_steps,
                "low_health_clears_queue",
                low_health_overrides_queue,
                low_health_overrides_queue ? "Low health retreat cleared queued orders." : low_health_auto.message);
            regression_ok = regression_ok && low_health_overrides_queue;
            hold_unit.health = hold_unit.max_health;
            hold_unit.automation_rules.clear();
            hold_unit.queued_orders.clear();
            hold_unit.assignment = {};
            hold_unit.current_order = dbd::UnitOrderType::Idle;
        }
    }

    if (bootstrap.starter_unit_ids.size() > 2 && !bootstrap.resource_node_ids.empty()) {
        const auto harvest_result = dbd_server::IssueHarvestOrder(
            world,
            bootstrap.player_id,
            bootstrap.starter_unit_ids.front(),
            bootstrap.resource_node_ids.front());
        std::cout << "Harvest test: " << harvest_result.message << std::endl;
        RecordStep(regression_steps, "harvest_order", harvest_result.ok, harvest_result.message);
        for (int i = 0; i < 120 && world.units.at(bootstrap.starter_unit_ids.front()).cargo.empty(); ++i) {
            dbd_server::ProgressUnitMovement(world, 0.5f);
        }
        const auto& harvested_unit = world.units.at(bootstrap.starter_unit_ids.front());
        const bool harvest_item_named =
            harvest_result.ok &&
            !harvested_unit.cargo.empty() &&
            dbd::FindItemDefinition(harvested_unit.cargo.back().resource_id) != nullptr;
        RecordStep(
            regression_steps,
            "harvest_named_item_cargo",
            harvest_item_named,
            harvest_item_named
                ? "Harvest generated cargo using a registered item definition."
                : "Harvest cargo did not map to a registered item definition.");
        regression_ok = regression_ok && harvest_item_named;

        const auto storage_before = world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.size();
        const auto return_result = dbd_server::IssueReturnToStorageOrder(
            world,
            bootstrap.player_id,
            bootstrap.starter_unit_ids.front(),
            bootstrap.depot_storage_id);
        if (return_result.ok) {
            auto& returning_unit = world.units.at(bootstrap.starter_unit_ids.front());
            returning_unit.position = world.storage_sites.at(bootstrap.depot_storage_id).position;
            returning_unit.move_target = returning_unit.position;
            const auto deliver_tick = dbd_server::ProgressUnitMovement(world, 1.0f);
            regression_ok = regression_ok && deliver_tick.ok;
        }
        const bool storage_increased =
            world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.size() > storage_before &&
            world.units.at(bootstrap.starter_unit_ids.front()).cargo.empty();
        RecordStep(
            regression_steps,
            "harvest_return_to_storage",
            harvest_result.ok && return_result.ok && storage_increased,
            storage_increased ? "Harvested cargo was delivered into storage." : return_result.message);
        regression_ok = regression_ok && harvest_result.ok && return_result.ok && storage_increased;

        auto& route_unit = world.units.at(bootstrap.starter_unit_ids[3]);
        route_unit.cargo.clear();
        route_unit.assignment = {};
        route_unit.current_order = dbd::UnitOrderType::Idle;
        route_unit.position = world.resource_nodes.at(bootstrap.resource_node_ids.front()).position;
        route_unit.move_target = route_unit.position;
        const auto route_result = dbd_server::IssueHaulRouteOrder(
            world,
            bootstrap.player_id,
            {route_unit.unit_id},
            dbd::HaulRouteSourceKind::ResourceNode,
            bootstrap.resource_node_ids.front(),
            bootstrap.depot_storage_id);
        const auto route_load_tick = dbd_server::ProgressHaulRoutes(world, 1.0f);
        const bool route_loaded =
            route_result.ok &&
            route_load_tick.ok &&
            !route_unit.cargo.empty() &&
            route_unit.assignment.route_active &&
            route_unit.assignment.route_phase == dbd::HaulRoutePhase::ToStorage;
        if (route_loaded) {
            route_unit.position = world.storage_sites.at(bootstrap.depot_storage_id).position;
            route_unit.move_target = route_unit.position;
        }
        const auto route_deliver_tick = dbd_server::ProgressHaulRoutes(world, 1.0f);
        const bool route_repeats =
            route_loaded &&
            route_deliver_tick.ok &&
            route_unit.cargo.empty() &&
            route_unit.assignment.route_active &&
            route_unit.assignment.route_phase == dbd::HaulRoutePhase::ToSource;
        RecordStep(
            regression_steps,
            "haul_route_resource_repeats",
            route_repeats,
            route_repeats ? "Haul route loaded resources, delivered them, and returned to source phase." : route_result.message);
        regression_ok = regression_ok && route_repeats;

        if (route_repeats && !raider_bootstrap.starter_unit_ids.empty()) {
            auto& raider_pressure = world.units.at(raider_bootstrap.starter_unit_ids.front());
            route_unit.position = {70.0f, 0.0f, 46.0f};
            route_unit.move_target = route_unit.position;
            route_unit.assignment.route_active = true;
            route_unit.assignment.route_phase = dbd::HaulRoutePhase::ToStorage;
            route_unit.cargo.push_back(dbd::CargoStack {90'101, 80, 600.0f, 3.0f, dbd::CargoCategory::Resource});
            raider_pressure.position = {route_unit.position.x + 5.0f, 0.0f, route_unit.position.z};
            raider_pressure.move_target = raider_pressure.position;
            const auto threat_result = dbd_server::RefreshRouteThreats(world);
            const bool route_threat_visible =
                threat_result.ok &&
                (route_unit.route_threat_level == "threatened" || route_unit.route_threat_level == "critical") &&
                route_unit.route_threat_enemy_id == raider_pressure.unit_id;
            RecordStep(
                regression_steps,
                "haul_route_threat_detected",
                route_threat_visible,
                route_threat_visible ? "Nearby raider raised route threat telemetry." : threat_result.message);
            regression_ok = regression_ok && route_threat_visible;

            if (bootstrap.starter_unit_ids.size() > 1) {
                auto& route_guard = world.units.at(bootstrap.starter_unit_ids[1]);
                const auto guard_route_result = dbd_server::IssueGuardUnitOrder(
                    world,
                    bootstrap.player_id,
                    {route_guard.unit_id},
                    route_unit.unit_id);
                const bool guarding_hauler =
                    guard_route_result.ok &&
                    route_guard.current_order == dbd::UnitOrderType::Escort &&
                    route_guard.assignment.target_entity_id == route_unit.unit_id &&
                    route_unit.assignment.route_active;
                RecordStep(
                    regression_steps,
                    "guard_route_hauler",
                    guarding_hauler,
                    guarding_hauler ? "Guard unit is escorting an active route hauler." : guard_route_result.message);
                regression_ok = regression_ok && guarding_hauler;
            }

            if (bootstrap.starter_unit_ids.size() > 2) {
                auto& route_responder = world.units.at(bootstrap.starter_unit_ids[2]);
                const auto guard_threat_result = dbd_server::IssueGuardThreatenedRouteOrder(
                    world,
                    bootstrap.player_id,
                    {route_responder.unit_id});
                const bool guard_threatened_route =
                    guard_threat_result.ok &&
                    route_responder.current_order == dbd::UnitOrderType::Escort &&
                    route_responder.assignment.target_entity_id == route_unit.unit_id;
                RecordStep(
                    regression_steps,
                    "guard_threatened_route",
                    guard_threatened_route,
                    guard_threatened_route ? "Responder attached to the most threatened route hauler." : guard_threat_result.message);
                regression_ok = regression_ok && guard_threatened_route;
            }

            if (bootstrap.starter_unit_ids.size() > 3) {
                auto& route_interceptor = world.units.at(bootstrap.starter_unit_ids[3]);
                const auto intercept_threat_result = dbd_server::IssueInterceptRouteThreatOrder(
                    world,
                    bootstrap.player_id,
                    {route_interceptor.unit_id});
                const bool intercepts_route_threat =
                    intercept_threat_result.ok &&
                    route_interceptor.assignment.target_entity_id == raider_pressure.unit_id;
                RecordStep(
                    regression_steps,
                    "intercept_route_threat",
                    intercepts_route_threat,
                    intercepts_route_threat ? "Responder moved to intercept the route threat enemy." : intercept_threat_result.message);
                regression_ok = regression_ok && intercepts_route_threat;
            }

            const auto route_stop_result = dbd_server::IssueStopOrder(
                world,
                bootstrap.player_id,
                {route_unit.unit_id});
            const bool stop_breaks_route =
                route_stop_result.ok &&
                !route_unit.assignment.route_active &&
                route_unit.current_order == dbd::UnitOrderType::Idle;
            RecordStep(
                regression_steps,
                "stop_interrupts_haul_route",
                stop_breaks_route,
                stop_breaks_route ? "Stop interrupted the active route." : route_stop_result.message);
            regression_ok = regression_ok && stop_breaks_route;

            const auto route_restart_result = dbd_server::IssueHaulRouteOrder(
                world,
                bootstrap.player_id,
                {route_unit.unit_id},
                dbd::HaulRouteSourceKind::ResourceNode,
                bootstrap.resource_node_ids.front(),
                bootstrap.depot_storage_id);
            const auto route_retreat_result = dbd_server::IssueRetreatOrder(
                world,
                bootstrap.player_id,
                {route_unit.unit_id});
            const bool retreat_breaks_route =
                route_restart_result.ok &&
                route_retreat_result.ok &&
                !route_unit.assignment.route_active &&
                route_unit.current_order == dbd::UnitOrderType::Retreat;
            RecordStep(
                regression_steps,
                "retreat_interrupts_haul_route",
                retreat_breaks_route,
                retreat_breaks_route ? "Retreat interrupted the active route and moved the hauler to safety." : route_retreat_result.message);
            regression_ok = regression_ok && retreat_breaks_route;

            route_unit.cargo.clear();
            route_unit.cargo.push_back(dbd::CargoStack {90'102, 40, 320.0f, 2.0f, dbd::CargoCategory::Resource});
            route_unit.assignment = {};
            route_unit.assignment.route_active = true;
            route_unit.assignment.route_phase = dbd::HaulRoutePhase::ToStorage;
            route_unit.current_order = dbd::UnitOrderType::HaulToStorage;
            const auto emergency_result = dbd_server::IssueEmergencyDepositOrder(
                world,
                bootstrap.player_id,
                {route_unit.unit_id});
            const bool emergency_started =
                emergency_result.ok &&
                !route_unit.assignment.route_active &&
                route_unit.current_order == dbd::UnitOrderType::HaulToStorage &&
                route_unit.assignment.target_entity_id == bootstrap.depot_storage_id;
            RecordStep(
                regression_steps,
                "emergency_deposit_interrupts_route",
                emergency_started,
                emergency_started ? "Emergency deposit broke the route and redirected cargo to storage." : emergency_result.message);
            regression_ok = regression_ok && emergency_started;
            const auto storage_before_emergency = world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.size();
            route_unit.position = world.storage_sites.at(bootstrap.depot_storage_id).position;
            route_unit.move_target = route_unit.position;
            const auto emergency_delivery_tick = dbd_server::ProgressUnitMovement(world, 1.0f);
            const bool emergency_delivered =
                emergency_delivery_tick.ok &&
                route_unit.cargo.empty() &&
                world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.size() > storage_before_emergency;
            RecordStep(
                regression_steps,
                "emergency_deposit_delivers_cargo",
                emergency_delivered,
                emergency_delivered ? "Emergency deposit delivered cargo into storage." : emergency_delivery_tick.message);
            regression_ok = regression_ok && emergency_delivered;

            route_unit.cargo.push_back(dbd::CargoStack {90'103, 32, 256.0f, 2.5f, dbd::CargoCategory::Resource});
            route_unit.assignment = {};
            route_unit.assignment.route_active = true;
            route_unit.assignment.route_phase = dbd::HaulRoutePhase::ToStorage;
            route_unit.current_order = dbd::UnitOrderType::HaulToStorage;
            const auto dropped_before = world.dropped_cargo.size();
            const auto drop_result = dbd_server::IssueDropCargoOrder(
                world,
                bootstrap.player_id,
                {route_unit.unit_id});
            dbd::Id dropped_route_cargo_id = 0;
            for (const auto& [drop_id, drop] : world.dropped_cargo) {
                if (drop.source_unit_id == route_unit.unit_id) {
                    dropped_route_cargo_id = drop_id;
                    break;
                }
            }
            const bool cargo_dropped =
                drop_result.ok &&
                !route_unit.assignment.route_active &&
                route_unit.current_order == dbd::UnitOrderType::Idle &&
                route_unit.cargo.empty() &&
                world.dropped_cargo.size() > dropped_before &&
                dropped_route_cargo_id != 0;
            RecordStep(
                regression_steps,
                "drop_cargo_creates_recoverable_pile",
                cargo_dropped,
                cargo_dropped ? "Drop cargo converted carried resources into a recoverable dropped pile." : drop_result.message);
            regression_ok = regression_ok && cargo_dropped;

            if (dropped_route_cargo_id != 0) {
                const auto relaunch_loot_result = dbd_server::IssueLootOrder(
                    world,
                    bootstrap.player_id,
                    route_unit.unit_id,
                    dropped_route_cargo_id);
                const bool dropped_cargo_relootable = relaunch_loot_result.ok && !route_unit.cargo.empty();
                RecordStep(
                    regression_steps,
                    "dropped_route_cargo_relootable",
                    dropped_cargo_relootable,
                    dropped_cargo_relootable ? "Dropped route cargo can be looted again." : relaunch_loot_result.message);
                regression_ok = regression_ok && dropped_cargo_relootable;
            }
        }

        auto& hauler = world.units.at(bootstrap.starter_unit_ids[2]);
        hauler.current_order = dbd::UnitOrderType::Idle;
        hauler.assignment = {};
        hauler.position = {24.0f, 0.0f, 8.0f};
        hauler.automation_rules.push_back(
            dbd::AutomationRule {dbd::AutomationTrigger::InventoryHeavy, dbd::AutomationAction::ReturnToStorage, 0.80f, true});
        hauler.cargo.push_back(dbd::CargoStack {90'001, 60, 420.0f, 5.0f, dbd::CargoCategory::Resource});
        std::cout << "Overburden test: cargo weight pushed near/over capacity for unit " << hauler.unit_id << std::endl;

        const auto automation_result = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Automation tick: " << automation_result.message << std::endl;
        regression_ok = regression_ok && automation_result.ok;
        const bool hauling_to_storage =
            world.units.at(bootstrap.starter_unit_ids[2]).current_order == dbd::UnitOrderType::HaulToStorage;
        RecordStep(
            regression_steps,
            "inventory_return_automation",
            automation_result.ok && hauling_to_storage,
            hauling_to_storage ? "Heavy inventory redirected unit to storage." : "Heavy inventory did not redirect unit to storage.");
        for (int i = 0; i < 6; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            std::cout << "Movement tick " << (i + 1) << ": " << movement_result.message << std::endl;
            regression_ok = regression_ok && movement_result.ok;
        }
    }

    if (!raider_bootstrap.starter_unit_ids.empty() && !bootstrap.starter_unit_ids.empty()) {
        auto& raider = world.units.at(raider_bootstrap.starter_unit_ids.front());
        auto& defender = world.units.at(bootstrap.starter_unit_ids[1]);
        raider.position = {defender.position.x + 4.0f, defender.position.y, defender.position.z + 2.0f};
        const auto enemy_auto = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Enemy detection automation: " << enemy_auto.message << std::endl;
        regression_ok = regression_ok && enemy_auto.ok;
        const bool attack_target_assigned =
            world.units.at(bootstrap.starter_unit_ids[1]).current_order == dbd::UnitOrderType::AttackTarget ||
            world.units.at(bootstrap.starter_unit_ids[3]).current_order == dbd::UnitOrderType::AttackTarget;
        RecordStep(
            regression_steps,
            "enemy_detection_attack_assignment",
            enemy_auto.ok && attack_target_assigned,
            attack_target_assigned ? "Enemy detection assigned an attack target." : "Enemy detection did not assign an attack target.");
        float total_enemy_health_before = 0.0f;
        for (dbd::Id unit_id : raider_bootstrap.starter_unit_ids) {
            total_enemy_health_before += world.units.at(unit_id).health;
        }
        for (int i = 0; i < 5; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            const auto combat_result = dbd_server::ProgressCombat(world, 1.0f);
            std::cout << "Combat pursuit tick " << (i + 1) << ": " << movement_result.message
                      << " | " << combat_result.message << std::endl;
            regression_ok = regression_ok && movement_result.ok && combat_result.ok;
        }
        float total_enemy_health_after = 0.0f;
        for (dbd::Id unit_id : raider_bootstrap.starter_unit_ids) {
            total_enemy_health_after += world.units.at(unit_id).health;
        }
        RecordStep(
            regression_steps,
            "combat_damage_applied",
            total_enemy_health_after < total_enemy_health_before,
            total_enemy_health_after < total_enemy_health_before ? "Combat reduced enemy squad health." : "Combat did not change enemy squad health.");

        if (bootstrap.starter_unit_ids.size() >= 3 && raider_bootstrap.starter_unit_ids.size() >= 3) {
            const dbd::Id short_id = bootstrap.starter_unit_ids[0];
            const dbd::Id mid_id = bootstrap.starter_unit_ids[1];
            const dbd::Id long_id = bootstrap.starter_unit_ids[2];
            const dbd::Id short_target_id = raider_bootstrap.starter_unit_ids[0];
            const dbd::Id mid_target_id = raider_bootstrap.starter_unit_ids[1];
            const dbd::Id long_target_id = raider_bootstrap.starter_unit_ids[2];

            auto prepare_attacker = [&](dbd::Id unit_id, dbd::CombatRangeBand band, dbd::Vec3 position) {
                auto& unit = world.units.at(unit_id);
                unit.alive = true;
                unit.permanently_dead = false;
                unit.health = unit.max_health;
                unit.position = position;
                unit.move_target = position;
                unit.current_order = dbd::UnitOrderType::Idle;
                unit.assignment = {};
                unit.combat_band = band;
            };
            auto prepare_target = [&](dbd::Id unit_id, dbd::Vec3 position) {
                auto& unit = world.units.at(unit_id);
                unit.alive = true;
                unit.permanently_dead = false;
                unit.health = unit.max_health;
                unit.position = position;
                unit.move_target = position;
                unit.current_order = dbd::UnitOrderType::Idle;
                unit.assignment = {};
            };

            prepare_attacker(short_id, dbd::CombatRangeBand::Short, {0.0f, 0.0f, 0.0f});
            prepare_target(short_target_id, {4.0f, 0.0f, 0.0f});
            prepare_attacker(mid_id, dbd::CombatRangeBand::Mid, {0.0f, 0.0f, 24.0f});
            prepare_target(mid_target_id, {10.0f, 0.0f, 24.0f});
            prepare_attacker(long_id, dbd::CombatRangeBand::Long, {0.0f, 0.0f, 48.0f});
            prepare_target(long_target_id, {16.0f, 0.0f, 48.0f});

            const float short_before = world.units.at(short_target_id).health;
            const float mid_before = world.units.at(mid_target_id).health;
            const float long_before = world.units.at(long_target_id).health;
            const auto short_attack = dbd_server::ResolveAttack(world, bootstrap.player_id, {short_id}, dbd::AttackTargetKind::Unit, short_target_id);
            const auto mid_attack = dbd_server::ResolveAttack(world, bootstrap.player_id, {mid_id}, dbd::AttackTargetKind::Unit, mid_target_id);
            const auto long_attack = dbd_server::ResolveAttack(world, bootstrap.player_id, {long_id}, dbd::AttackTargetKind::Unit, long_target_id);
            const auto band_combat = dbd_server::ProgressCombat(world, 1.0f);
            const float short_damage = short_before - world.units.at(short_target_id).health;
            const float mid_damage = mid_before - world.units.at(mid_target_id).health;
            const float long_damage = long_before - world.units.at(long_target_id).health;
            const bool band_damage_order =
                short_attack.ok && mid_attack.ok && long_attack.ok && band_combat.ok &&
                short_damage > mid_damage && mid_damage > long_damage && long_damage > 0.0f;
            RecordStep(
                regression_steps,
                "combat_band_damage_profile",
                band_damage_order,
                band_damage_order ? "Short > Mid > Long damage profile held at valid ranges." : "Combat band damage profile did not separate as expected.");
            regression_ok = regression_ok && band_damage_order;

            prepare_attacker(short_id, dbd::CombatRangeBand::Short, {0.0f, 0.0f, 72.0f});
            prepare_target(short_target_id, {16.0f, 0.0f, 72.0f});
            prepare_attacker(mid_id, dbd::CombatRangeBand::Mid, {0.0f, 0.0f, 96.0f});
            prepare_target(mid_target_id, {16.0f, 0.0f, 96.0f});
            prepare_attacker(long_id, dbd::CombatRangeBand::Long, {0.0f, 0.0f, 120.0f});
            prepare_target(long_target_id, {16.0f, 0.0f, 120.0f});
            const float short_range_before = world.units.at(short_target_id).health;
            const float mid_range_before = world.units.at(mid_target_id).health;
            const float long_range_before = world.units.at(long_target_id).health;
            dbd_server::ResolveAttack(world, bootstrap.player_id, {short_id}, dbd::AttackTargetKind::Unit, short_target_id);
            dbd_server::ResolveAttack(world, bootstrap.player_id, {mid_id}, dbd::AttackTargetKind::Unit, mid_target_id);
            dbd_server::ResolveAttack(world, bootstrap.player_id, {long_id}, dbd::AttackTargetKind::Unit, long_target_id);
            const auto range_combat = dbd_server::ProgressCombat(world, 1.0f);
            const bool long_range_only =
                range_combat.ok &&
                world.units.at(short_target_id).health == short_range_before &&
                world.units.at(mid_target_id).health == mid_range_before &&
                world.units.at(long_target_id).health < long_range_before;
            RecordStep(
                regression_steps,
                "combat_band_range_profile",
                long_range_only,
                long_range_only ? "Long band hit at 16m while Short/Mid were still closing." : "Combat range profile did not separate as expected.");
            regression_ok = regression_ok && long_range_only;

            auto& fast_retreat = world.units.at(short_id);
            auto& heavy_retreat = world.units.at(mid_id);
            fast_retreat.cargo.clear();
            heavy_retreat.cargo = {dbd::CargoStack {9991, 80, 80.0f, 2.0f, dbd::CargoCategory::Resource}};
            fast_retreat.position = {0.0f, 0.0f, 144.0f};
            heavy_retreat.position = {0.0f, 0.0f, 156.0f};
            fast_retreat.move_target = {20.0f, 0.0f, 144.0f};
            heavy_retreat.move_target = {20.0f, 0.0f, 156.0f};
            fast_retreat.current_order = dbd::UnitOrderType::Retreat;
            heavy_retreat.current_order = dbd::UnitOrderType::Retreat;
            fast_retreat.stamina = fast_retreat.max_stamina;
            heavy_retreat.stamina = heavy_retreat.max_stamina;
            const auto fast_start = fast_retreat.position;
            const auto heavy_start = heavy_retreat.position;
            const auto retreat_move = dbd_server::ProgressUnitMovement(world, 1.0f);
            const bool overburden_retreat_penalty =
                retreat_move.ok &&
                Distance(fast_start, world.units.at(short_id).position) > Distance(heavy_start, world.units.at(mid_id).position);
            RecordStep(
                regression_steps,
                "overburden_retreat_penalty",
                overburden_retreat_penalty,
                overburden_retreat_penalty ? "Clean retreat moved farther than overloaded retreat." : "Overburden did not reduce retreat movement.");
            regression_ok = regression_ok && overburden_retreat_penalty;
        }
    }

    if (bootstrap.starter_unit_ids.size() > 2) {
        auto& wounded = world.units.at(bootstrap.starter_unit_ids[2]);
        wounded.health = 18.0f;
        const auto retreat_auto = dbd_server::EvaluateAutomation(world, 1.0f);
        std::cout << "Low-health retreat automation: " << retreat_auto.message << std::endl;
        regression_ok = regression_ok && retreat_auto.ok;
        for (int i = 0; i < 4; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            std::cout << "Retreat movement tick " << (i + 1) << ": " << movement_result.message << std::endl;
            regression_ok = regression_ok && movement_result.ok;
        }
    }

    const dbd::Vec3 worksite_position {112.0f, 0.0f, 96.0f};
    const auto flatten_result = dbd_server::StartFlattenJob(world, bootstrap.player_id, worksite_position, 8.0f, 0.14f);
    std::cout << "Flatten start: " << flatten_result.message << std::endl;
    const bool flatten_not_required = flatten_result.message.find("already buildable") != std::string::npos;
    regression_ok = regression_ok && (flatten_result.ok || flatten_not_required);
    float flatten_labor_after = 0.0f;
    if (flatten_result.ok) {
        const auto assign_flatten = dbd_server::AssignUnitsToFlattenJob(
            world,
            bootstrap.player_id,
            flatten_result.entity_id,
            {bootstrap.starter_unit_ids[0], bootstrap.starter_unit_ids[1]});
        std::cout << "Flatten assignment: " << assign_flatten.message << std::endl;
        regression_ok = regression_ok && assign_flatten.ok;

        std::size_t assigned_before_interrupt = world.flatten_jobs.at(flatten_result.entity_id).assigned_unit_ids.size();
        const auto interrupt_move = dbd_server::IssueMoveOrder(
            world,
            bootstrap.player_id,
            {bootstrap.starter_unit_ids[1]},
            dbd::Vec3 {65.0f, 0.0f, 35.0f});
        const std::size_t assigned_after_interrupt = world.flatten_jobs.at(flatten_result.entity_id).assigned_unit_ids.size();
        RecordStep(
            regression_steps,
            "flatten_worker_removed_by_move",
            assign_flatten.ok && interrupt_move.ok && assigned_after_interrupt < assigned_before_interrupt,
            assigned_after_interrupt < assigned_before_interrupt ? "Move command removed unit from flatten labor." : interrupt_move.message);
        regression_ok = regression_ok && interrupt_move.ok && (assigned_after_interrupt < assigned_before_interrupt);

        for (int i = 0; i < 220; ++i) {
            const auto flatten_progress = dbd_server::ProgressFlattenJobs(world, 1.0f);
            regression_ok = regression_ok && flatten_progress.ok;
            if (world.flatten_jobs.at(flatten_result.entity_id).state == dbd::FlattenJobStateKind::Completed) {
                break;
            }
        }
        flatten_labor_after = world.flatten_jobs.at(flatten_result.entity_id).accumulated_labor;
    }
    RecordStep(
        regression_steps,
        "flatten_progress",
        flatten_result.ok ? (world.flatten_jobs.at(flatten_result.entity_id).state == dbd::FlattenJobStateKind::Completed) : flatten_not_required,
        flatten_result.ok ? "Flatten completed and stamped terrain." : flatten_result.message);

    const auto construction_result = dbd_server::StartConstruction(
        world,
        bootstrap.player_id,
        dbd::StructureType::Extractor,
        worksite_position,
        8.0f);
    std::cout << "Construction start: " << construction_result.message << std::endl;
    regression_ok = regression_ok && construction_result.ok;
    float site_labor_after = 0.0f;
    float site_health_before_raid = 0.0f;
    float site_health_after_raid = 0.0f;

    if (construction_result.ok) {
        for (int i = 0; i < 8; ++i) {
            const auto site_progress = dbd_server::ProgressConstructionSites(world, 1.0f);
            regression_ok = regression_ok && site_progress.ok;
        }
        site_labor_after = world.construction_sites.at(construction_result.entity_id).accumulated_labor;
        site_health_before_raid = world.construction_sites.at(construction_result.entity_id).health;

        auto& raider_a = world.units.at(raider_bootstrap.starter_unit_ids[0]);
        auto& raider_b = world.units.at(raider_bootstrap.starter_unit_ids[1]);
        const auto site_position = world.construction_sites.at(construction_result.entity_id).position;
        raider_a.position = {site_position.x + 4.0f, site_position.y, site_position.z + 1.0f};
        raider_b.position = {site_position.x + 5.0f, site_position.y, site_position.z - 1.0f};

        const auto raid_result = dbd_server::ResolveAttack(
            world,
            raider_bootstrap.player_id,
            {raider_bootstrap.starter_unit_ids[0], raider_bootstrap.starter_unit_ids[1]},
            dbd::AttackTargetKind::ConstructionSite,
            construction_result.entity_id);
        std::cout << "Raid test: " << raid_result.message << std::endl;
        regression_ok = regression_ok && raid_result.ok;
        for (int i = 0; i < 6; ++i) {
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            const auto combat_result = dbd_server::ProgressCombat(world, 1.0f);
            std::cout << "Worksite pressure tick " << (i + 1) << ": " << movement_result.message
                      << " | " << combat_result.message << std::endl;
            regression_ok = regression_ok && movement_result.ok && combat_result.ok;
        }
        if (world.construction_sites.find(construction_result.entity_id) != world.construction_sites.end()) {
            site_health_after_raid = world.construction_sites.at(construction_result.entity_id).health;
        }

        for (int i = 0; i < 24; ++i) {
            const auto site_progress = dbd_server::ProgressConstructionSites(world, 1.0f);
            regression_ok = regression_ok && site_progress.ok;
        }
    }
    RecordStep(
        regression_steps,
        "construction_progress",
        construction_result.ok && site_labor_after > 0.0f,
        construction_result.ok ? "Automatic construction progress increased." : construction_result.message);
    RecordStep(
        regression_steps,
        "construction_under_attack",
        construction_result.ok && site_health_after_raid < site_health_before_raid,
        construction_result.ok ? "Raid pressure reduced construction site health." : construction_result.message);
    regression_ok = regression_ok && (!construction_result.ok || site_health_after_raid < site_health_before_raid);

    if (construction_result.ok && world.construction_sites.find(construction_result.entity_id) != world.construction_sites.end()) {
        auto& repair_site = world.construction_sites.at(construction_result.entity_id);
        repair_site.health = std::max(1.0f, repair_site.health - 30.0f);
        const float site_health_before_repair = repair_site.health;
        auto& site_repair_unit = world.units.at(bootstrap.starter_unit_ids[0]);
        site_repair_unit.position = repair_site.position;
        site_repair_unit.move_target = repair_site.position;
        const auto repair_site_order = dbd_server::IssueRepairConstructionSiteOrder(
            world,
            bootstrap.player_id,
            {site_repair_unit.unit_id},
            construction_result.entity_id);
        const auto repair_site_progress = dbd_server::ProgressRepairOrders(world, 1.0f);
        const bool site_repaired =
            repair_site_order.ok &&
            repair_site_progress.ok &&
            world.construction_sites.at(construction_result.entity_id).health > site_health_before_repair;
        RecordStep(
            regression_steps,
            "repair_construction_site",
            site_repaired,
            site_repaired ? "Construction site health increased from repair labor." : repair_site_order.message);
        regression_ok = regression_ok && site_repaired;

        repair_site.health = std::max(1.0f, repair_site.health - 20.0f);
        auto& stalled_repair_unit = world.units.at(bootstrap.starter_unit_ids[1]);
        stalled_repair_unit.position = repair_site.position;
        stalled_repair_unit.move_target = repair_site.position;
        const auto previous_storage_resources = world.storage_sites.at(bootstrap.depot_storage_id).stored_resources;
        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.clear();
        const auto stalled_order = dbd_server::IssueRepairConstructionSiteOrder(
            world,
            bootstrap.player_id,
            {stalled_repair_unit.unit_id},
            construction_result.entity_id);
        const auto stalled_progress = dbd_server::ProgressRepairOrders(world, 1.0f);
        const bool repair_stalled =
            stalled_order.ok &&
            stalled_progress.ok &&
            world.units.at(stalled_repair_unit.unit_id).tactical_state == "Repair stalled: no materials";
        RecordStep(
            regression_steps,
            "repair_stalls_without_storage_materials",
            repair_stalled,
            repair_stalled ? "Repair stayed active but stalled without stored materials." : stalled_order.message);
        regression_ok = regression_ok && repair_stalled;

        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources = previous_storage_resources;
        const auto supply_drops_before = world.dropped_cargo.size();
        const float site_health_before_supply_repair = repair_site.health;
        auto& supply_unit = world.units.at(bootstrap.starter_unit_ids[3]);
        supply_unit.position = {
            repair_site.position.x - 4.0f,
            repair_site.position.y,
            repair_site.position.z - 4.0f};
        supply_unit.move_target = supply_unit.position;
        const auto supply_repair_order = dbd_server::IssueSupplyRepairOrder(
            world,
            bootstrap.player_id,
            {supply_unit.unit_id},
            dbd::AttackTargetKind::ConstructionSite,
            construction_result.entity_id);
        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources.clear();
        stalled_repair_unit.position = repair_site.position;
        stalled_repair_unit.move_target = repair_site.position;
        const auto supplied_repair_progress = dbd_server::ProgressRepairOrders(world, 1.0f);
        const bool supply_pile_created = world.dropped_cargo.size() > supply_drops_before;
        const bool supply_repair_resumed =
            supply_repair_order.ok &&
            supplied_repair_progress.ok &&
            supply_pile_created &&
            world.construction_sites.at(construction_result.entity_id).health > site_health_before_supply_repair;
        RecordStep(
            regression_steps,
            "supply_repair_resumes_stalled_repair",
            supply_repair_resumed,
            supply_repair_resumed
                ? "Repair supply staged nearby materials and stalled repair resumed."
                : (supply_repair_order.ok ? supplied_repair_progress.message : supply_repair_order.message));
        regression_ok = regression_ok && supply_repair_resumed;
        world.storage_sites.at(bootstrap.depot_storage_id).stored_resources = previous_storage_resources;

        const auto stop_repair_result = dbd_server::IssueStopOrder(world, bootstrap.player_id, {stalled_repair_unit.unit_id});
        const bool stop_interrupts_repair =
            stop_repair_result.ok &&
            world.units.at(stalled_repair_unit.unit_id).current_order == dbd::UnitOrderType::Idle &&
            world.units.at(stalled_repair_unit.unit_id).assignment.kind == dbd::JobKind::None;
        RecordStep(
            regression_steps,
            "stop_interrupts_repair",
            stop_interrupts_repair,
            stop_interrupts_repair ? "Stop cleared repair order and assignment." : stop_repair_result.message);
        regression_ok = regression_ok && stop_interrupts_repair;
    }

    if (world.structures.find(bootstrap.depot_structure_id) != world.structures.end()) {
        auto& repair_depot = world.structures.at(bootstrap.depot_structure_id);
        repair_depot.health = std::max(1.0f, repair_depot.health - 35.0f);
        const float depot_health_before_repair = repair_depot.health;
        auto& depot_repair_unit = world.units.at(bootstrap.starter_unit_ids[2]);
        depot_repair_unit.position = repair_depot.position;
        depot_repair_unit.move_target = repair_depot.position;
        const auto repair_structure_order = dbd_server::IssueRepairStructureOrder(
            world,
            bootstrap.player_id,
            {depot_repair_unit.unit_id},
            bootstrap.depot_structure_id);
        const auto repair_structure_progress = dbd_server::ProgressRepairOrders(world, 1.0f);
        const bool structure_repaired =
            repair_structure_order.ok &&
            repair_structure_progress.ok &&
            world.structures.at(bootstrap.depot_structure_id).health > depot_health_before_repair;
        RecordStep(
            regression_steps,
            "repair_structure",
            structure_repaired,
            structure_repaired ? "Structure health increased from repair labor." : repair_structure_order.message);
        regression_ok = regression_ok && structure_repaired;

        const auto foreign_repair = dbd_server::IssueRepairStructureOrder(
            world,
            raider_bootstrap.player_id,
            {raider_bootstrap.starter_unit_ids.front()},
            bootstrap.depot_structure_id);
        RecordStep(
            regression_steps,
            "repair_foreign_structure_rejected",
            !foreign_repair.ok,
            !foreign_repair.ok ? foreign_repair.message : "Foreign repair was unexpectedly accepted.");
        regression_ok = regression_ok && !foreign_repair.ok;

        const auto retreat_repair_order = dbd_server::IssueRepairStructureOrder(
            world,
            bootstrap.player_id,
            {depot_repair_unit.unit_id},
            bootstrap.depot_structure_id);
        const auto retreat_repair_result = dbd_server::IssueRetreatOrder(world, bootstrap.player_id, {depot_repair_unit.unit_id});
        const bool retreat_interrupts_repair =
            retreat_repair_order.ok &&
            retreat_repair_result.ok &&
            world.units.at(depot_repair_unit.unit_id).current_order == dbd::UnitOrderType::Retreat;
        RecordStep(
            regression_steps,
            "retreat_interrupts_repair",
            retreat_interrupts_repair,
            retreat_interrupts_repair ? "Retreat replaced repair order." : retreat_repair_result.message);
        regression_ok = regression_ok && retreat_interrupts_repair;
    }

    const float credits_before_econ = world.players.at(bootstrap.player_id).credits;
    const auto carried_cargo_before_econ = world.units.at(bootstrap.starter_unit_ids[2]).cargo;
    const auto economic_result = dbd_server::ProcessEconomicSettlement(world, 10.0f);
    std::cout << "Economic settlement: " << economic_result.message << std::endl;
    regression_ok = regression_ok && economic_result.ok;
    const auto& economy_player = world.players.at(bootstrap.player_id);
    RecordStep(
        regression_steps,
        "economic_settlement",
        economic_result.ok && (economy_player.food_load >= 0.0f) && (economy_player.credits <= credits_before_econ),
        economic_result.ok ? "Economic loads and credits updated." : economic_result.message);
    RecordStep(
        regression_steps,
        "economic_uses_storage_not_carried_cargo",
        carried_cargo_before_econ.size() == world.units.at(bootstrap.starter_unit_ids[2]).cargo.size(),
        carried_cargo_before_econ.size() == world.units.at(bootstrap.starter_unit_ids[2]).cargo.size()
            ? "Economic settlement left carried cargo untouched."
            : "Economic settlement unexpectedly consumed carried cargo.");
    regression_ok = regression_ok &&
        (carried_cargo_before_econ.size() == world.units.at(bootstrap.starter_unit_ids[2]).cargo.size());

    auto& depot = world.structures.at(bootstrap.depot_structure_id);
    dbd::Id depot_storage_id_for_raid = bootstrap.depot_storage_id;
    if (world.storage_sites.find(depot_storage_id_for_raid) == world.storage_sites.end()) {
        auto& restored_storage = dbd_server::CreateStorageSite(
            world,
            depot.structure_id,
            bootstrap.player_id,
            depot.position);
        restored_storage.stored_resources.push_back(
            dbd::CargoStack {80'099, 45, 240.0f, 1.2f, dbd::CargoCategory::Resource});
        depot_storage_id_for_raid = restored_storage.storage_site_id;
    }
    for (std::size_t i = 0; i < raider_bootstrap.starter_unit_ids.size(); ++i) {
        auto& raider_striker = world.units.at(raider_bootstrap.starter_unit_ids[i]);
        raider_striker.position = {
            depot.position.x + 3.0f + static_cast<float>(i),
            depot.position.y,
            depot.position.z + ((i % 2 == 0) ? 1.0f : -1.0f)};
    }
    const auto dropped_before_depot_raid = world.dropped_cargo.size();
    const auto storage_present_before_raid = world.storage_sites.find(depot_storage_id_for_raid) != world.storage_sites.end();
    const auto depot_raid_order = dbd_server::ResolveAttack(
        world,
        raider_bootstrap.player_id,
        raider_bootstrap.starter_unit_ids,
        dbd::AttackTargetKind::Structure,
        bootstrap.depot_structure_id);
    for (int i = 0; i < 90 && world.structures.find(bootstrap.depot_structure_id) != world.structures.end(); ++i) {
        const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
        const auto combat_result = dbd_server::ProgressCombat(world, 1.0f);
        regression_ok = regression_ok && movement_result.ok && combat_result.ok;
    }
    const bool depot_destroyed = world.structures.find(bootstrap.depot_structure_id) == world.structures.end();
    const bool dropped_cargo_spawned = world.dropped_cargo.size() > dropped_before_depot_raid;
    RecordStep(
        regression_steps,
        "storage_destruction_drops_and_destroys",
        storage_present_before_raid && depot_destroyed && dropped_cargo_spawned,
        depot_destroyed && dropped_cargo_spawned
            ? "Depot raid destroyed storage and spawned dropped cargo."
            : depot_raid_order.message);
    regression_ok = regression_ok &&
        storage_present_before_raid && depot_destroyed && dropped_cargo_spawned;

    const auto first_drop_it = world.dropped_cargo.begin();
    const dbd::Id loot_drop_id = first_drop_it != world.dropped_cargo.end() ? first_drop_it->first : 0;
    const dbd::Id looter_unit_id = raider_bootstrap.starter_unit_ids[2];
    const auto looter_cargo_before = world.units.at(looter_unit_id).cargo.size();
    const auto dropped_count_before_loot = world.dropped_cargo.size();
    const auto loot_result = dbd_server::IssueLootOrder(
        world,
        raider_bootstrap.player_id,
        looter_unit_id,
        loot_drop_id);
    for (int i = 0; i < 120 && world.units.at(looter_unit_id).cargo.size() == looter_cargo_before; ++i) {
        dbd_server::ProgressUnitMovement(world, 0.5f);
    }
    const auto looter_cargo_after = world.units.at(looter_unit_id).cargo.size();
    const bool loot_removed_drop = world.dropped_cargo.size() < dropped_count_before_loot;
    RecordStep(
        regression_steps,
        "loot_dropped_cargo",
        loot_result.ok && looter_cargo_after > looter_cargo_before && loot_removed_drop,
        loot_result.ok && loot_removed_drop
            ? "Raider looted dropped storage cargo into unit inventory."
            : loot_result.message);
    regression_ok = regression_ok && loot_result.ok && looter_cargo_after > looter_cargo_before && loot_removed_drop;

    const auto looter_cargo_before_unstored_econ = world.units.at(looter_unit_id).cargo;
    const auto unstored_econ_result = dbd_server::ProcessEconomicSettlement(world, 10.0f);
    const bool unstored_loot_preserved =
        world.units.at(looter_unit_id).cargo.size() == looter_cargo_before_unstored_econ.size() &&
        CargoWeight(world.units.at(looter_unit_id).cargo) == CargoWeight(looter_cargo_before_unstored_econ);
    RecordStep(
        regression_steps,
        "loot_cargo_not_used_before_storage",
        unstored_econ_result.ok && unstored_loot_preserved,
        unstored_loot_preserved
            ? "Loot cargo stayed on the unit during economy settlement."
            : "Unstored loot cargo was unexpectedly consumed.");
    regression_ok = regression_ok && unstored_econ_result.ok && unstored_loot_preserved;

    const auto raider_storage_it = world.storage_sites.find(raider_bootstrap.depot_storage_id);
    const auto raider_storage_stacks_before =
        raider_storage_it != world.storage_sites.end() ? raider_storage_it->second.stored_resources.size() : 0;
    const auto return_loot_result = dbd_server::IssueReturnToStorageOrder(
        world,
        raider_bootstrap.player_id,
        looter_unit_id,
        raider_bootstrap.depot_storage_id);
    if (return_loot_result.ok) {
        auto& looter = world.units.at(looter_unit_id);
        const auto storage_after_return_order = world.storage_sites.find(raider_bootstrap.depot_storage_id);
        if (storage_after_return_order != world.storage_sites.end()) {
            looter.position = storage_after_return_order->second.position;
            looter.move_target = looter.position;
            const auto movement_result = dbd_server::ProgressUnitMovement(world, 1.0f);
            regression_ok = regression_ok && movement_result.ok;
        }
    }
    const auto raider_storage_after_it = world.storage_sites.find(raider_bootstrap.depot_storage_id);
    const auto raider_storage_stacks_after =
        raider_storage_after_it != world.storage_sites.end() ? raider_storage_after_it->second.stored_resources.size() : 0;
    RecordStep(
        regression_steps,
        "looted_cargo_returned_to_storage",
        return_loot_result.ok && raider_storage_stacks_after > raider_storage_stacks_before,
        return_loot_result.ok && raider_storage_stacks_after > raider_storage_stacks_before
            ? "Looted cargo became stored value after return-to-storage."
            : return_loot_result.message);
    regression_ok = regression_ok && return_loot_result.ok && raider_storage_stacks_after > raider_storage_stacks_before;

    auto& heavy_drop = dbd_server::CreateDroppedCargo(
        world,
        0,
        {82.0f, 0.0f, 42.0f},
        {dbd::CargoStack {88'888, 44, 520.0f, 2.0f, dbd::CargoCategory::Resource}});
    const dbd::Id heavy_drop_id = heavy_drop.dropped_cargo_id;
    dbd::Id heavy_looter_id = 0;
    for (dbd::Id candidate_id : raider_bootstrap.starter_unit_ids) {
        const auto candidate_it = world.units.find(candidate_id);
        if (candidate_it != world.units.end() && candidate_it->second.alive && !candidate_it->second.permanently_dead) {
            heavy_looter_id = candidate_id;
            break;
        }
    }
    if (heavy_looter_id == 0) {
        heavy_looter_id = bootstrap.starter_unit_ids.front();
    }
    auto& heavy_looter = world.units.at(heavy_looter_id);
    heavy_looter.cargo.clear();
    heavy_looter.alive = true;
    heavy_looter.permanently_dead = false;
    heavy_looter.health = heavy_looter.max_health;
    heavy_looter.position = heavy_drop.position;
    heavy_looter.move_target = heavy_looter.position;
    heavy_looter.current_order = dbd::UnitOrderType::Idle;
    heavy_looter.assignment = {};
    const dbd::Id heavy_looter_player_id = heavy_looter.owner_player_id;
    const auto heavy_loot_result = dbd_server::IssueLootOrder(
        world,
        heavy_looter_player_id,
        heavy_looter_id,
        heavy_drop_id);
    const float heavy_cargo_weight = CargoWeight(heavy_looter.cargo);
    const bool heavy_overburdened = heavy_loot_result.ok && heavy_cargo_weight > heavy_looter.carry_capacity;
    const dbd::Vec3 heavy_start = heavy_looter.position;
    const auto heavy_move_order = dbd_server::IssueMoveOrder(
        world,
        heavy_looter_player_id,
        {heavy_looter_id},
        {heavy_start.x + 80.0f, heavy_start.y, heavy_start.z});
    const auto heavy_move_tick = dbd_server::ProgressUnitMovement(world, 1.0f);
    const float heavy_distance_moved = Distance(heavy_start, heavy_looter.position);
    const bool overburden_penalty_applied =
        heavy_overburdened && heavy_move_order.ok && heavy_move_tick.ok &&
        heavy_distance_moved < (heavy_looter.base_move_speed * 0.95f);
    RecordStep(
        regression_steps,
        "overburden_loot_slows_hauler",
        overburden_penalty_applied,
        overburden_penalty_applied
            ? "Heavy looted cargo slowed the hauler below base movement speed."
            : "Overburden movement penalty did not apply to looted cargo.");
    regression_ok = regression_ok && overburden_penalty_applied;

    const auto drops_before_loaded_death = world.dropped_cargo.size();
    const auto loaded_death_result = dbd_server::ResolvePermanentUnitDeath(world, heavy_looter_id);
    const bool loaded_death_redropped =
        loaded_death_result.ok && world.dropped_cargo.size() > drops_before_loaded_death;
    RecordStep(
        regression_steps,
        "loaded_hauler_death_redrops_loot",
        loaded_death_redropped,
        loaded_death_redropped
            ? "Loaded hauler death re-ran cargo drop/destruction."
            : loaded_death_result.message);
    regression_ok = regression_ok && loaded_death_redropped;

    const auto death_result = dbd_server::ResolvePermanentUnitDeath(world, bootstrap.starter_unit_ids.back());
    std::cout << "Permanent death test: " << death_result.message << std::endl;

    dbd_server::PerformDailyChunkMaintenance(world);
    std::cout << "Maintenance test complete. Persistent world state kept; resources/drops/corpses in opened chunks reset." << std::endl;

    const auto save_root = std::filesystem::path(DBD_REBOOT_ROOT) / "runtime-save";
    std::filesystem::create_directories(save_root);
    const auto command_spool = dbd_server::PrepareCommandSpool(save_root);
    std::cout << "World save root: " << save_root.string() << std::endl;
    std::cout << "Command spool path: " << command_spool.root.string() << std::endl;
    const bool save_ok = dbd_server::SaveWorldState(world, save_root);
    std::cout << "Save test: " << (save_ok ? "world saved." : "save failed.") << std::endl;
    regression_ok = regression_ok && save_ok;
    RecordStep(
        regression_steps,
        "save_world",
        save_ok,
        save_ok ? "World save completed." : dbd_server::GetLastWorldStateDiagnostic());

    dbd_server::WorldState loaded_world;
    const bool load_ok = dbd_server::LoadWorldState(loaded_world, save_root);
    std::cout << "Load test: " << (load_ok ? "world loaded." : "load failed.") << std::endl;
    regression_ok = regression_ok && load_ok;
    RecordStep(
        regression_steps,
        "load_world",
        load_ok,
        load_ok ? "World load completed." : dbd_server::GetLastWorldStateDiagnostic());
    if (load_ok) {
        const auto loaded_unit_it = loaded_world.units.find(bootstrap.starter_unit_ids.front());
        const auto loaded_squad_it = loaded_world.squads.find(bootstrap.squad_id);
        const bool unit_rules_persisted =
            loaded_unit_it != loaded_world.units.end() &&
            loaded_unit_it->second.automation_rules.size() == veteran_rules.size();
        const bool squad_rules_persisted =
            loaded_squad_it != loaded_world.squads.end() &&
            loaded_squad_it->second.automation_rules.size() == squad_rules.size();
        RecordStep(
            regression_steps,
            "automation_rules_persist_after_reload",
            unit_rules_persisted && squad_rules_persisted,
            unit_rules_persisted && squad_rules_persisted
                ? "Unit and squad automation rules survived save/load."
                : "Automation rules were lost across save/load.");
        regression_ok = regression_ok && unit_rules_persisted && squad_rules_persisted;
    }

    const auto snapshot_path = save_root / "world_snapshot.json";
    if (load_ok) {
        dbd_server::RefreshChunkStreaming(loaded_world);
    } else {
        dbd_server::RefreshChunkStreaming(world);
    }
    const bool snapshot_ok = dbd_server::ExportWorldSnapshotJson(load_ok ? loaded_world : world, snapshot_path);
    std::cout << "Snapshot export: " << (snapshot_ok ? snapshot_path.string() : "failed") << std::endl;
    regression_ok = regression_ok && snapshot_ok;
    RecordStep(
        regression_steps,
        "export_snapshot",
        snapshot_ok,
        snapshot_ok ? "Snapshot export completed." : "Snapshot export failed.");

    const auto regression_report_path = save_root / "regression_report.json";
    const bool report_ok = WriteRegressionReportJson(
        regression_report_path,
        regression_ok,
        load_ok ? loaded_world.tick : world.tick,
        save_root,
        command_spool.root,
        snapshot_path,
        regression_steps);
    std::cout << "Regression report: " << (report_ok ? regression_report_path.string() : "failed") << std::endl;
    regression_ok = regression_ok && report_ok;
    RecordStep(
        regression_steps,
        "export_regression_report",
        report_ok,
        report_ok ? "Regression report export completed." : "Regression report export failed.");

    std::cout << "Regression summary: " << (regression_ok ? "PASS" : "FAIL") << std::endl;
    return 0;
}
