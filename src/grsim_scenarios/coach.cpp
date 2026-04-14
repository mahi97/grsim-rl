/**
 * Coach compiler: converts strategic commands into per-robot skills.
 *
 * The coach layer is architecture-neutral — it provides a declarative
 * interface that compiles down through skills to body velocity actions.
 * This allows STP, FSM, behavior tree, and RL users to all map onto it.
 */

#include "grsim_scenarios/coach.h"
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <algorithm>

namespace grsim_scenarios {

using namespace grsim_core;

// ============================================================
// Formation definitions
// ============================================================

std::vector<FormationPosition> CoachCompiler::getFormationPositions(
    Formation formation, int num_robots
) {
    std::vector<FormationPosition> positions;

    switch (formation) {
        case Formation::DEFENSIVE_4_1:
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.6, 0.5, Role::DEFENDER_LEFT},
                {-0.6, 0.15, Role::DEFENDER_CENTER},
                {-0.6, -0.15, Role::DEFENDER_CENTER},
                {-0.6, -0.5, Role::DEFENDER_RIGHT},
                {-0.2, 0.0, Role::MIDFIELDER_CENTER},
            };
            break;

        case Formation::BALANCED_3_2:
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.5, 0.4, Role::DEFENDER_LEFT},
                {-0.5, 0.0, Role::DEFENDER_CENTER},
                {-0.5, -0.4, Role::DEFENDER_RIGHT},
                {-0.1, 0.25, Role::MIDFIELDER_LEFT},
                {-0.1, -0.25, Role::MIDFIELDER_RIGHT},
            };
            break;

        case Formation::ATTACKING_2_3:
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.5, 0.3, Role::DEFENDER_LEFT},
                {-0.5, -0.3, Role::DEFENDER_RIGHT},
                {0.1, 0.4, Role::ATTACKER_LEFT},
                {0.1, 0.0, Role::ATTACKER_CENTER},
                {0.1, -0.4, Role::ATTACKER_RIGHT},
            };
            break;

        case Formation::WIDE_2_2_1:
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.5, 0.4, Role::DEFENDER_LEFT},
                {-0.5, -0.4, Role::DEFENDER_RIGHT},
                {-0.1, 0.6, Role::MIDFIELDER_LEFT},
                {-0.1, -0.6, Role::MIDFIELDER_RIGHT},
                {0.2, 0.0, Role::ATTACKER_CENTER},
            };
            break;

        case Formation::KICKOFF:
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.5, 0.3, Role::DEFENDER_LEFT},
                {-0.5, -0.3, Role::DEFENDER_RIGHT},
                {-0.2, 0.4, Role::MIDFIELDER_LEFT},
                {-0.2, -0.4, Role::MIDFIELDER_RIGHT},
                {-0.05, 0.0, Role::ATTACKER_CENTER},
            };
            break;

        default: // DEFAULT
            positions = {
                {-0.9, 0.0, Role::GOALKEEPER},
                {-0.5, 0.3, Role::DEFENDER_LEFT},
                {-0.5, 0.0, Role::DEFENDER_CENTER},
                {-0.5, -0.3, Role::DEFENDER_RIGHT},
                {-0.1, 0.2, Role::MIDFIELDER_CENTER},
                {0.1, 0.0, Role::ATTACKER_CENTER},
            };
            break;
    }

    // Trim to actual robot count
    if (static_cast<int>(positions.size()) > num_robots) {
        positions.resize(num_robots);
    }

    return positions;
}

// ============================================================
// Role → skill mapping
// ============================================================

static std::unique_ptr<Skill> skillForRole(
    Role role,
    const WorldState& state,
    int team,
    const CoachCommand& cmd,
    double pos_x, double pos_y
) {
    double half_l = 6.0; // approximate
    double sign = (team == 0) ? -1.0 : 1.0;

    switch (role) {
        case Role::GOALKEEPER:
            // Stay near own goal, face ball
            return std::make_unique<GoToPoseSkill>(
                sign * (half_l - 0.3), 0.0,
                std::atan2(state.ball.y, state.ball.x - sign * half_l),
                0.1, 0.2
            );

        case Role::ATTACKER_CENTER:
        case Role::ATTACKER_LEFT:
        case Role::ATTACKER_RIGHT: {
            // If near ball, try to get it and shoot
            double dist_to_ball = std::sqrt(
                (pos_x - state.ball.x) * (pos_x - state.ball.x) +
                (pos_y - state.ball.y) * (pos_y - state.ball.y)
            );
            if (dist_to_ball < 2.0) {
                double goal_x = -sign * half_l;
                return std::make_unique<KickToPointSkill>(goal_x, 0.0, 5.0);
            }
            // Otherwise go to formation position
            return std::make_unique<GoToPoseSkill>(
                pos_x, pos_y,
                std::atan2(state.ball.y - pos_y, state.ball.x - pos_x),
                0.3, 0.3
            );
        }

        case Role::MIDFIELDER_LEFT:
        case Role::MIDFIELDER_CENTER:
        case Role::MIDFIELDER_RIGHT:
            // Go to formation, face ball
            return std::make_unique<GoToPoseSkill>(
                pos_x, pos_y,
                std::atan2(state.ball.y - pos_y, state.ball.x - pos_x),
                0.3, 0.3
            );

        case Role::DEFENDER_LEFT:
        case Role::DEFENDER_CENTER:
        case Role::DEFENDER_RIGHT: {
            // Block lane from ball to own goal
            double goal_x = sign * half_l;
            return std::make_unique<BlockLaneSkill>(
                state.ball.x, state.ball.y, goal_x, 0.0
            );
        }

        default:
            return std::make_unique<GoToPoseSkill>(pos_x, pos_y, 0.0);
    }
}

// ============================================================
// Coach compiler
// ============================================================

std::vector<CoachCompiler::SkillAssignment> CoachCompiler::compile(
    const CoachCommand& command,
    const WorldState& state,
    int team
) {
    std::vector<SkillAssignment> assignments;
    const auto& robots = (team == 0) ? state.blue_robots : state.yellow_robots;

    auto positions = getFormationPositions(command.formation, robots.size());

    double half_l = 6.0; // approximate
    double half_w = 4.5;
    double sign = (team == 0) ? 1.0 : -1.0;

    for (size_t i = 0; i < robots.size() && i < positions.size(); i++) {
        const auto& r = robots[i];
        const auto& fp = positions[i];

        // Convert ratio to field coordinates
        double px = sign * fp.x_ratio * half_l;
        double py = fp.y_ratio * half_w;

        // Check if role is overridden
        Role role = fp.role;
        auto it = command.role_assignments.find(r.id);
        if (it != command.role_assignments.end()) {
            role = it->second;
        }

        SkillAssignment sa;
        sa.robot_id = r.id;
        sa.skill = skillForRole(role, state, team, command, px, py);
        sa.trace = "coach(" + std::to_string(static_cast<int>(command.formation)) +
                   ") → role(" + std::to_string(static_cast<int>(role)) +
                   ") → " + sa.skill->name();
        assignments.push_back(std::move(sa));
    }

    return assignments;
}

TeamActions CoachCompiler::toTeamActions(
    const CoachCommand& command,
    const WorldState& state,
    int team,
    double dt
) {
    TeamActions actions;
    auto assignments = compile(command, state, team);
    const auto& robots = (team == 0) ? state.blue_robots : state.yellow_robots;

    actions.robot_actions.resize(robots.size());

    for (auto& sa : assignments) {
        for (size_t i = 0; i < robots.size(); i++) {
            if (robots[i].id == sa.robot_id) {
                auto result = sa.skill->execute(state, team, sa.robot_id, dt);
                actions.robot_actions[i] = result.action;
                break;
            }
        }
    }

    return actions;
}

} // namespace grsim_scenarios
