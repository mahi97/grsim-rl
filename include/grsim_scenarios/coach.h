#ifndef GRSIM_SCENARIOS_COACH_H
#define GRSIM_SCENARIOS_COACH_H

#include "grsim_core/world_state.h"
#include "grsim_scenarios/skills.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace grsim_scenarios {

/**
 * Coach-level action interface.
 *
 * Compiles strategic decisions into per-robot skill assignments.
 * The coach layer is intentionally architecture-neutral — it does not
 * force STP, FSM, or behavior trees. It provides a declarative API
 * for role assignment, formation selection, and high-level play calls.
 */

// ---- Roles ----

enum class Role {
    GOALKEEPER = 0,
    DEFENDER_LEFT,
    DEFENDER_CENTER,
    DEFENDER_RIGHT,
    MIDFIELDER_LEFT,
    MIDFIELDER_CENTER,
    MIDFIELDER_RIGHT,
    ATTACKER_LEFT,
    ATTACKER_CENTER,
    ATTACKER_RIGHT,
    FREE_ROLE,
};

// ---- Formations ----

enum class Formation {
    DEFAULT = 0,
    DEFENSIVE_4_1,    // 4 defenders + 1 midfielder
    BALANCED_3_2,     // 3 defenders + 2 midfielders
    ATTACKING_2_3,    // 2 defenders + 3 attackers
    WIDE_2_2_1,       // Wide formation for possession
    KICKOFF,
    PENALTY_ATTACK,
    PENALTY_DEFEND,
};

// ---- Set-piece plays ----

enum class SetPiece {
    NONE = 0,
    KICKOFF_SHORT_PASS,
    KICKOFF_LONG_PASS,
    FREEKICK_DIRECT_SHOT,
    FREEKICK_PASS_AND_SHOOT,
    FREEKICK_CHIP_TO_CORNER,
    CORNER_KICK_NEAR_POST,
    CORNER_KICK_FAR_POST,
    GOALKICK_SHORT,
    GOALKICK_LONG,
    THROWIN_QUICK,
};

// ---- Attack patterns ----

enum class AttackPattern {
    NONE = 0,
    DIRECT_ATTACK,       // Rush ball toward goal
    POSSESSION_BUILDUP,  // Short passes, maintain possession
    WING_PLAY,           // Use wide positions
    COUNTER_ATTACK,      // Fast transition from defense
    SET_PIECE_ROUTINE,   // Execute planned set piece
};

// ---- Pressing level ----

enum class PressingLevel {
    PASSIVE = 0,   // Stay in shape, wait for opponent
    MODERATE,      // Press when ball is in certain zones
    HIGH_PRESS,    // Aggressive pressing everywhere
};

// ---- Transition mode ----

enum class TransitionMode {
    BALANCED = 0,
    ATTACK_PRIORITY,   // Commit bodies forward on transition
    DEFENSE_PRIORITY,  // Drop back quickly on loss
};

// ---- Coach command: a single strategic decision ----

struct CoachCommand {
    // Role assignment: robot_id → role
    std::map<int, Role> role_assignments;

    // Formation
    Formation formation = Formation::DEFAULT;

    // Set piece to execute (if applicable)
    SetPiece set_piece = SetPiece::NONE;

    // Attack pattern
    AttackPattern attack_pattern = AttackPattern::NONE;

    // Pressing level
    PressingLevel pressing = PressingLevel::MODERATE;

    // Transition mode
    TransitionMode transition = TransitionMode::BALANCED;

    // Team-wide constraints
    bool allow_chip_kicks = true;
    double max_dribble_distance = 0.8;  // meters (safety margin vs 1m rule)
    bool keeper_can_leave_area = false;
};

// ---- Formation positions (relative to field) ----

struct FormationPosition {
    double x_ratio;  // -1.0 (own goal) to 1.0 (opponent goal)
    double y_ratio;  // -1.0 (bottom) to 1.0 (top)
    Role role;
};

// ---- Coach compiler: coach command → skill assignments ----

class CoachCompiler {
public:
    struct SkillAssignment {
        int robot_id;
        std::unique_ptr<Skill> skill;
        std::string trace;
    };

    // Compile a coach command into per-robot skill assignments
    static std::vector<SkillAssignment> compile(
        const CoachCommand& command,
        const grsim_core::WorldState& state,
        int team
    );

    // Get formation positions for a given formation
    static std::vector<FormationPosition> getFormationPositions(
        Formation formation,
        int num_robots
    );

    // Convert a coach command to Actions (for direct engine stepping)
    static grsim_core::TeamActions toTeamActions(
        const CoachCommand& command,
        const grsim_core::WorldState& state,
        int team,
        double dt
    );
};

} // namespace grsim_scenarios

#endif // GRSIM_SCENARIOS_COACH_H
