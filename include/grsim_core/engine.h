#ifndef GRSIM_CORE_ENGINE_H
#define GRSIM_CORE_ENGINE_H

#include "grsim_core/config.h"
#include "grsim_core/world_state.h"

#include <cstdint>
#include <memory>
#include <random>
#include <functional>

namespace grsim_core {

// Forward declarations for ODE types we don't want to expose
struct EngineImpl;

// Event types emitted during stepping
enum class EventType {
    NONE = 0,
    BALL_OUT_TOUCHLINE,
    BALL_OUT_GOALLINE,
    GOAL_SCORED_BLUE,
    GOAL_SCORED_YELLOW,
    ROBOT_COLLISION,
    BALL_KICKED,
    BALL_CHIPPED,
    DRIBBLER_CONTACT,
    DRIBBLER_RELEASE,
};

struct SimEvent {
    EventType type = EventType::NONE;
    double timestamp = 0.0;
    int team = -1;     // relevant team (-1 if N/A)
    int robot_id = -1; // relevant robot (-1 if N/A)
    double x = 0.0, y = 0.0;  // position where event occurred
};

// Step result returned by the engine
struct StepResult {
    WorldState state;
    std::vector<SimEvent> events;
    bool ball_in_play = true;
};

// Callback for custom collision handling
using CollisionCallback = std::function<void(int obj1_type, int obj1_id, int obj2_type, int obj2_id)>;

class SimulationEngine {
public:
    explicit SimulationEngine(const SimConfig& config);
    ~SimulationEngine();

    // Non-copyable
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    // Move-constructible
    SimulationEngine(SimulationEngine&&) noexcept;
    SimulationEngine& operator=(SimulationEngine&&) noexcept;

    // --- Core simulation interface ---

    // Step the simulation forward by dt seconds (uses config delta_time if dt <= 0)
    StepResult step(const Actions& actions, double dt = -1.0);

    // Step without actions (free-running physics)
    StepResult stepPhysics(double dt = -1.0);

    // --- State management ---

    // Get current world state
    WorldState getState() const;

    // Get flattened observation vector
    FlatObservation observe() const;

    // Reset to default formation with given seed
    void reset(uint64_t seed = 0);

    // Reset with specific initial state
    void reset(uint64_t seed, const WorldState& initial_state);

    // Snapshot/restore (serializable state)
    WorldState snapshot() const;
    void restore(const WorldState& state);

    // --- Direct control interface ---

    void setWheelSpeeds(int team, int robot_id, double w0, double w1, double w2, double w3);
    void setBodyVelocity(int team, int robot_id, double vx, double vy, double vw);
    void setGlobalVelocity(int team, int robot_id, double vx, double vy, double vw);
    void kick(int team, int robot_id, double speed, double angle_deg = 0.0);
    void setDribbler(int team, int robot_id, bool on);

    // --- Teleportation (for scenario setup) ---

    void teleportBall(double x, double y, double z = 0.0,
                      double vx = 0.0, double vy = 0.0, double vz = 0.0);
    void teleportRobot(int team, int robot_id,
                       double x, double y, double orientation_rad,
                       bool present = true);

    // --- Robot management ---

    void setRobotPresent(int team, int robot_id, bool present);
    int robotsPerTeam() const;

    // --- Configuration ---

    const SimConfig& config() const;
    double simTime() const;
    int frameNumber() const;

    // --- Field geometry helpers ---

    bool isBallInField() const;
    bool isBallInGoal(int team) const;  // 0=blue goal, 1=yellow goal
    bool isRobotInDefenseArea(int team, int robot_id) const;
    bool isPositionInDefenseArea(int defense_team, double x, double y) const;
    bool isPositionInField(double x, double y) const;

private:
    std::unique_ptr<EngineImpl> impl_;
};

} // namespace grsim_core

#endif // GRSIM_CORE_ENGINE_H
