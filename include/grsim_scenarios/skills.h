#ifndef GRSIM_SCENARIOS_SKILLS_H
#define GRSIM_SCENARIOS_SKILLS_H

#include "grsim_core/world_state.h"
#include "grsim_core/engine.h"
#include <cmath>
#include <string>
#include <vector>

namespace grsim_scenarios {

/**
 * Skill-level action interface.
 *
 * Each skill compiles down to body-velocity (or wheel) actions.
 * Skills maintain internal state and produce a RobotAction each tick.
 * The decomposition trace is kept for debugging.
 */

struct SkillResult {
    grsim_core::RobotAction action;
    bool complete = false;
    std::string trace;  // Debug decomposition path
};

// Base skill class
class Skill {
public:
    virtual ~Skill() = default;
    virtual SkillResult execute(
        const grsim_core::WorldState& state,
        int team, int robot_id,
        double dt
    ) = 0;
    virtual std::string name() const = 0;
    virtual void reset() {}
};

// ---- Concrete skills ----

class GoToPoseSkill : public Skill {
    double target_x_, target_y_, target_ori_;
    double pos_tolerance_, ori_tolerance_;
    double kp_pos_, kp_ori_;
public:
    GoToPoseSkill(double x, double y, double ori,
                   double pos_tol = 0.05, double ori_tol = 0.05)
        : target_x_(x), target_y_(y), target_ori_(ori),
          pos_tolerance_(pos_tol), ori_tolerance_(ori_tol),
          kp_pos_(3.0), kp_ori_(5.0) {}

    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "go_to_pose"; }
};

class FacePointSkill : public Skill {
    double target_x_, target_y_;
public:
    FacePointSkill(double x, double y) : target_x_(x), target_y_(y) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "face_point"; }
};

class KickToPointSkill : public Skill {
    double target_x_, target_y_;
    double kick_speed_;
    bool chip_;
public:
    KickToPointSkill(double x, double y, double speed = 5.0, bool chip = false)
        : target_x_(x), target_y_(y), kick_speed_(speed), chip_(chip) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return chip_ ? "chip_to_point" : "kick_to_point"; }
};

class ReceiveBallSkill : public Skill {
public:
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "receive_ball"; }
};

class InterceptBallSkill : public Skill {
public:
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "intercept_ball"; }
};

class DribbleToPointSkill : public Skill {
    double target_x_, target_y_;
public:
    DribbleToPointSkill(double x, double y) : target_x_(x), target_y_(y) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "dribble_to_point"; }
};

class MarkRobotSkill : public Skill {
    int mark_team_, mark_id_;
public:
    MarkRobotSkill(int team, int id) : mark_team_(team), mark_id_(id) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "mark_robot"; }
};

class BlockLaneSkill : public Skill {
    double from_x_, from_y_, to_x_, to_y_;
public:
    BlockLaneSkill(double fx, double fy, double tx, double ty)
        : from_x_(fx), from_y_(fy), to_x_(tx), to_y_(ty) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "block_lane"; }
};

class PlaceBallSkill : public Skill {
    double target_x_, target_y_;
public:
    PlaceBallSkill(double x, double y) : target_x_(x), target_y_(y) {}
    SkillResult execute(const grsim_core::WorldState& state, int team, int robot_id, double dt) override;
    std::string name() const override { return "place_ball"; }
};

// ---- Skill compiler: skill → body velocity action ----

class SkillCompiler {
public:
    // Convert a skill command to a robot action for this tick
    static grsim_core::RobotAction compile(
        Skill& skill,
        const grsim_core::WorldState& state,
        int team, int robot_id,
        double dt
    ) {
        auto result = skill.execute(state, team, robot_id, dt);
        return result.action;
    }
};

} // namespace grsim_scenarios

#endif // GRSIM_SCENARIOS_SKILLS_H
