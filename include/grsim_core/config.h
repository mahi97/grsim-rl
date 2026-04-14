#ifndef GRSIM_CORE_CONFIG_H
#define GRSIM_CORE_CONFIG_H

#include <string>
#include <cmath>

namespace grsim_core {

struct FieldConfig {
    double field_length = 12.0;       // meters (Division A)
    double field_width = 9.0;
    double field_line_width = 0.010;
    double center_circle_radius = 0.5;
    double free_kick_distance = 0.7;
    double penalty_area_depth = 1.8;
    double penalty_area_width = 3.6;
    double penalty_mark_distance = 8.0;
    double goal_width = 1.8;
    double goal_depth = 0.18;
    double goal_height = 0.16;
    double goal_thickness = 0.012;
    double margin_touch_line = 0.3;
    double margin_goal_line = 0.3;
    double wall_thickness = 0.050;
    double referee_margin = 0.425;
    double goal_substitution_area_width = 2.0;
};

struct BallConfig {
    double radius = 0.0215;
    double mass = 0.046;
    double friction = 0.7;
    double slip = 1.0;
    double bounce = 0.5;
    double bounce_vel = 0.1;
    double linear_damp = 0.004;
    double angular_damp = 0.004;
    bool project_airborne = false;
    // Two-phase ball model
    double model_acc_slide = -6.2;
    double model_acc_roll = -0.7;
    double model_k_switch = 0.69;
    // Chip model
    double chip_damping_xy_first_hop = 0.8;
    double chip_damping_xy_other_hops = 0.6;
    double chip_damping_z = 0.5;
};

struct RobotConfig {
    // Geometry
    double center_from_kicker = 0.073;
    double radius = 0.09;
    double height = 0.14;
    double bottom_height = 0.02;
    double kicker_z = 0.005;
    double kicker_thickness = 0.005;
    double kicker_width = 0.08;
    double kicker_height = 0.04;
    double wheel_radius = 0.027;
    double wheel_thickness = 0.005;
    double wheel1_angle = 60.0;
    double wheel2_angle = 135.0;
    double wheel3_angle = 225.0;
    double wheel4_angle = 300.0;
    // Physics
    double body_mass = 2.0;
    double wheel_mass = 0.02;
    double kicker_mass = 0.02;
    double kicker_damp_factor = 0.2;
    double roller_torque_factor = 0.06;
    double roller_perpendicular_torque_factor = 0.005;
    double kicker_friction = 0.8;
    double wheel_tangent_friction = 0.8;
    double wheel_perpendicular_friction = 1.0;
    double wheel_motor_fmax = 0.2;
    double max_linear_kick_speed = 6.5;
    double max_chip_kick_speed = 6.5;
    // Dynamics limits
    double acc_speedup_absolute_max = 3.5;
    double acc_speedup_angular_max = 30.0;
    double acc_brake_absolute_max = 5.0;
    double acc_brake_angular_max = 40.0;
    double vel_absolute_max = 3.0;
    double vel_angular_max = 20.0;
};

struct SimulationParams {
    double delta_time = 0.016;    // 60 FPS default
    double gravity = 9.81;
    int robots_per_team = 11;
    int ball_collision_substeps = 5;
};

struct NoiseConfig {
    bool enabled = false;
    double deviation_x = 0.0;
    double deviation_y = 0.0;
    double deviation_angle = 0.0;
    bool vanishing_enabled = false;
    double ball_vanishing = 0.0;
    double blue_vanishing = 0.0;
    double yellow_vanishing = 0.0;
};

struct SimConfig {
    FieldConfig field;
    BallConfig ball;
    RobotConfig blue_robot;
    RobotConfig yellow_robot;
    SimulationParams sim;
    NoiseConfig noise;

    static SimConfig defaults();
    static SimConfig divisionA();
    static SimConfig divisionB();
};

} // namespace grsim_core

#endif // GRSIM_CORE_CONFIG_H
