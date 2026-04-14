# Refactor Plan: grSim → grsim-rl

## Strategy Overview

The refactor follows a **wrap-and-extract** strategy rather than a full rewrite:

1. Extract pure simulation logic into `grsim_core` library
2. Keep existing Qt/GUI code working by having it call into `grsim_core`
3. Build new layers (ref, Python) on top of `grsim_core`
4. Preserve backward compatibility for existing grSim users

## Phase 1: Configuration Decoupling

### Goal: Pure-data configuration independent of Qt/VarTypes

**Current state:** `ConfigWidget` inherits `VarTreeView` (Qt widget). All configuration
access goes through a GUI object. Robot settings are in `RobotSettings` struct (good).

**Actions:**
1. Create `SimConfig` struct in `grsim_core/config.h` containing:
   - `FieldConfig` (dimensions, division)
   - `BallConfig` (radius, mass, friction, damping, ball model params)
   - `RobotSettings` (already exists, move it)
   - `SimulationConfig` (delta_time, gravity, robot_count)
   - `NoiseConfig` (deviation_x/y/angle, vanishing params)
2. Add `SimConfig SimConfig::fromDefaults()` factory
3. Add `SimConfig SimConfig::fromJSON(const std::string& path)` loader
4. Make `ConfigWidget` populate a `SimConfig` and pass it down
5. All simulation code references `SimConfig` instead of `ConfigWidget*`

### Files affected:
- New: `include/grsim_core/config.h`, `src/grsim_core/config.cpp`
- Modified: `include/configwidget.h`, `include/sslworld.h`, `include/robot.h`

## Phase 2: Graphics Decoupling

### Goal: Physics objects work without OpenGL/CGraphics

**Current state:** `PWorld` takes `CGraphics*`. Every `PObject` has `draw()` and `glinit()`.
`SSLWorld::step()` interleaves rendering with physics.

**Actions:**
1. Make `CGraphics*` optional (nullptr = headless) in `PWorld`
2. Guard all `draw()` / `glinit()` calls with null checks
3. Split `SSLWorld::step()` into:
   - `stepPhysics(dReal dt)` — pure physics, no rendering
   - `stepRender()` — all drawing code
   - `step(dReal dt)` — calls both (backward compat)
4. Remove `QImage` from `Robot` class (move to render layer)
5. Remove OpenGL from `Robot::drawLabel()` (move to render layer)

### Files affected:
- Modified: `src/sslworld.cpp`, `src/physics/pworld.cpp`, `src/robot.cpp`
- Modified: all physics headers

## Phase 3: Core World State Object

### Goal: Serializable world state for snapshot/restore/observation

**Actions:**
1. Define `WorldState` struct:
   ```cpp
   struct BallState { double x, y, z, vx, vy, vz; };
   struct RobotState { 
     int id; int team; bool on;
     double x, y, orientation;
     double vx, vy, vw;
     double wheel_speeds[4];
     bool dribbler_on; bool touching_ball;
   };
   struct WorldState {
     double sim_time;
     BallState ball;
     std::vector<RobotState> robots;
   };
   ```
2. Add `WorldState SSLWorld::getState() const`
3. Add `void SSLWorld::setState(const WorldState& state)`
4. Add `WorldState SSLWorld::snapshot() const` (full ODE state if feasible, else WorldState)
5. Add `void SSLWorld::restore(const WorldState& snapshot)`

### Note on ODE state serialization:
ODE does not support full state serialization natively. Our approach:
- `WorldState` captures positions, velocities, orientations
- `restore()` rebuilds by teleporting all objects and setting velocities
- This is slightly lossy (contact forces, internal joint states lost)
- Acceptable for RL episode resets (always start from clean state)

## Phase 4: Deterministic Reset

### Goal: `reset(seed)` produces identical initial states

**Actions:**
1. Replace global `rand0_1()` / `randn_*()` with seeded RNG
2. Create `SimRNG` class wrapping `std::mt19937`
3. `reset(uint64_t seed)`:
   - Destroy and recreate ODE world (cleanest approach)
   - OR teleport all objects to initial positions and zero velocities
4. Provide `reset(uint64_t seed, ScenarioConfig scenario)` for scenario-based reset

### Decision: Rebuild vs Teleport
- **Rebuild**: Guarantees clean ODE state, but slow (~1-5ms)
- **Teleport**: Fast but may have residual contact/joint artifacts
- **Choice**: Support both, default to teleport, option to rebuild

## Phase 5: Eliminate Global State

### Goal: Multiple SSLWorld instances can coexist

**Actions:**
1. Replace `SSLWorld* _w` global with user-data pointer in ODE callbacks
   - ODE `dSpaceCollide` already passes `void* data` — use it
   - Collision callbacks access world via the data parameter
2. Make `ROBOT_BLUE_CHASSIS_COLOR` / `ROBOT_YELLOW_CHASSIS_COLOR` instance members
3. Move random functions to instance-level `SimRNG`

## Phase 6: Step Without Event Loop

### Goal: Direct `step()` callable without QApplication/QTimer

**Actions:**
1. Create `SimulationEngine` class (the grsim_core public API):
   ```cpp
   class SimulationEngine {
   public:
     SimulationEngine(const SimConfig& config);
     ~SimulationEngine();
     
     void step(const Actions& actions, dReal dt);
     WorldState getState() const;
     Observations observe() const;
     void reset(uint64_t seed);
     void reset(uint64_t seed, const ScenarioConfig& scenario);
     WorldState snapshot() const;
     void restore(const WorldState& state);
     
     // Direct action interfaces
     void setWheelSpeeds(int team, int robot, double w1, double w2, double w3, double w4);
     void setBodyVelocity(int team, int robot, double vx, double vy, double vw);
     void kick(int team, int robot, double speed, double angle);
     void setDribbler(int team, int robot, bool on);
     void teleportBall(double x, double y, double z, double vx, double vy, double vz);
     void teleportRobot(int team, int robot, double x, double y, double orientation);
   };
   ```
2. `SimulationEngine` owns the ODE world, robots, ball — no Qt dependencies
3. Existing `SSLWorld` becomes a thin wrapper that:
   - Owns a `SimulationEngine` instance
   - Adds networking (UDP sockets)
   - Adds rendering (CGraphics)
   - Connects to Qt event loop

## Phase 7: CMake Restructuring

### Goal: Separate build targets for core, realtime, Python

**New CMake structure:**
```
CMakeLists.txt (top-level)
├── src/grsim_core/CMakeLists.txt    → libgrsim_core.a
│   Dependencies: ODE only
├── src/grsim_realtime/CMakeLists.txt → grSim executable
│   Dependencies: libgrsim_core + Qt5 + OpenGL + VarTypes + Protobuf
├── src/grsim_ref/CMakeLists.txt     → libgrsim_ref.a
│   Dependencies: libgrsim_core
├── python/CMakeLists.txt            → pygrsim.so (pybind11)
│   Dependencies: libgrsim_core + libgrsim_ref + pybind11
└── tests/CMakeLists.txt             → test executables
    Dependencies: libgrsim_core + libgrsim_ref + Catch2/GoogleTest
```

## Execution Order

| Step | Description | Risk | Effort |
|------|-------------|------|--------|
| 1 | Extract SimConfig from ConfigWidget | Low | Medium |
| 2 | Split SSLWorld::step() into physics/render | Low | Small |
| 3 | Make CGraphics optional in PWorld/PObject | Low | Small |
| 4 | Define WorldState and observe/snapshot | Low | Small |
| 5 | Eliminate global _w pointer | Medium | Small |
| 6 | Create SimulationEngine wrapping core logic | Medium | Large |
| 7 | Add seeded RNG and reset() | Low | Small |
| 8 | Restructure CMake for library targets | Medium | Medium |
| 9 | Implement Python bindings | Medium | Large |
| 10 | Add event detector framework | Low | Medium |
| 11 | Add scenario system | Low | Medium |
| 12 | Add hierarchical actions | Low | Large |

## Backward Compatibility Contract

1. `grSim` executable continues to work identically for existing users
2. Legacy grSim protocol (port 20011) continues to work
3. SSL simulation protocol continues to work
4. Vision multicast output continues to work
5. Configuration INI files continue to work
6. Command-line flags continue to work
7. GUI behavior unchanged

All new functionality is additive. No existing feature is removed.
