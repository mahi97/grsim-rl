/**
 * Smoke test for grsim_core: create engine, step, observe, reset.
 * Build: cmake ... && cmake --build . --target test_core
 * Run: ./test_core
 */

#include "grsim_core/engine.h"
#include "grsim_core/config.h"
#include "grsim_core/world_state.h"
#include "grsim_ref/events.h"
#include "grsim_ref/reward_compiler.h"
#include "grsim_scenarios/scenario.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>

using namespace grsim_core;
using namespace grsim_ref;

int main() {
    std::cout << "=== grsim_core smoke test ===" << std::endl;

    // 1. Create config
    auto config = SimConfig::divisionA();
    config.sim.robots_per_team = 3; // Small for speed
    std::cout << "[PASS] SimConfig created (DivA, 3v3)" << std::endl;

    // 2. Create engine
    SimulationEngine engine(config);
    std::cout << "[PASS] SimulationEngine created" << std::endl;

    // 3. Get initial state
    auto state = engine.getState();
    assert(state.blue_robots.size() == 3);
    assert(state.yellow_robots.size() == 3);
    std::cout << "[PASS] Initial state: " << state.totalRobots() << " robots" << std::endl;

    // 4. Step with empty actions
    Actions empty_actions;
    auto result = engine.step(empty_actions, 0.016);
    assert(result.state.sim_time > 0);
    std::cout << "[PASS] Step executed, sim_time=" << result.state.sim_time << std::endl;

    // 5. Observe
    auto obs = engine.observe();
    int expected_dim = 6 + 6 * 7; // ball(6) + 6 robots * 7
    assert(obs.data.size() == expected_dim);
    std::cout << "[PASS] Observation size=" << obs.data.size() << std::endl;

    // 6. Teleport ball
    engine.teleportBall(1.0, 2.0, 0.0, 3.0, 0.0, 0.0);
    auto s2 = engine.getState();
    assert(std::abs(s2.ball.x - 1.0) < 0.01);
    assert(std::abs(s2.ball.y - 2.0) < 0.01);
    std::cout << "[PASS] Teleport ball to (1,2)" << std::endl;

    // 7. Teleport robot
    engine.teleportRobot(0, 0, 3.0, 1.5, 0.0);
    auto s3 = engine.getState();
    assert(std::abs(s3.blue_robots[0].x - 3.0) < 0.1);
    std::cout << "[PASS] Teleport robot" << std::endl;

    // 8. Step 100 times
    for (int i = 0; i < 100; i++) {
        engine.stepPhysics(0.016);
    }
    auto s4 = engine.getState();
    std::cout << "[PASS] 100 physics steps, sim_time=" << s4.sim_time << std::endl;

    // 9. Snapshot and restore
    auto snap = engine.snapshot();
    engine.stepPhysics(0.016);
    auto s5 = engine.getState();
    assert(s5.sim_time != snap.sim_time);
    engine.restore(snap);
    auto s6 = engine.getState();
    assert(std::abs(s6.sim_time - snap.sim_time) < 0.001);
    std::cout << "[PASS] Snapshot/restore" << std::endl;

    // 10. Reset
    engine.reset(42);
    auto s7 = engine.getState();
    assert(s7.sim_time == 0.0);
    std::cout << "[PASS] Reset(seed=42)" << std::endl;

    // 11. Field queries
    assert(engine.isPositionInField(0, 0));
    assert(!engine.isPositionInField(100, 0));
    assert(engine.isPositionInDefenseArea(1, 5.5, 0));  // Yellow defense area
    std::cout << "[PASS] Field geometry queries" << std::endl;

    // 12. Event detection
    auto registry = EventDetectorRegistry::createDefault(config);
    auto prev = engine.getState();
    engine.teleportBall(0, 10, 0); // Way out of field
    auto curr = engine.getState();
    auto events = registry.detectAll(curr, prev, 0.016);
    std::cout << "[PASS] Event detector ran, " << events.size() << " events detected" << std::endl;

    // 13. Reward compiler
    auto reward_compiler = RewardCompiler::scoringProfile();
    auto reward = reward_compiler.compute(curr, prev, events, 0);
    std::cout << "[PASS] Reward compiled: " << reward.reward << std::endl;

    // 14. Throughput test
    engine.reset(0);
    config.sim.robots_per_team = 6;
    SimulationEngine engine2(config);
    int steps = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < steps; i++) {
        engine2.stepPhysics(0.016);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    double sps = steps / elapsed;
    std::cout << "[PASS] Throughput: " << (int)sps << " steps/sec (6v6, dt=16ms)" << std::endl;

    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}
