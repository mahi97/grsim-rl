#include "grsim_core/config.h"

namespace grsim_core {

SimConfig SimConfig::defaults() {
    return divisionA();
}

SimConfig SimConfig::divisionA() {
    SimConfig cfg;
    // Field - Division A (2024 rules)
    cfg.field.field_length = 12.0;
    cfg.field.field_width = 9.0;
    cfg.field.center_circle_radius = 0.5;
    cfg.field.free_kick_distance = 0.7;
    cfg.field.penalty_area_depth = 1.8;
    cfg.field.penalty_area_width = 3.6;
    cfg.field.penalty_mark_distance = 8.0;
    cfg.field.goal_width = 1.8;
    cfg.field.goal_depth = 0.18;
    cfg.field.goal_height = 0.16;
    cfg.field.margin_touch_line = 0.3;
    cfg.field.margin_goal_line = 0.3;
    cfg.field.referee_margin = 0.425;
    cfg.field.wall_thickness = 0.050;
    cfg.field.goal_substitution_area_width = 2.0;

    // Simulation
    cfg.sim.robots_per_team = 11;
    cfg.sim.delta_time = 0.016;
    cfg.sim.gravity = 9.81;
    cfg.sim.ball_collision_substeps = 5;

    return cfg;
}

SimConfig SimConfig::divisionB() {
    SimConfig cfg = divisionA();
    // Field - Division B (smaller)
    cfg.field.field_length = 9.0;
    cfg.field.field_width = 6.0;
    cfg.field.penalty_area_depth = 1.0;
    cfg.field.penalty_area_width = 2.0;
    cfg.field.penalty_mark_distance = 6.0;
    cfg.field.goal_width = 1.0;
    cfg.sim.robots_per_team = 6;
    return cfg;
}

} // namespace grsim_core
