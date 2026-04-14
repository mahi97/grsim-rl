# Paper Outline: grsim-rl

## Proposed Title
**grsim-rl: A Gymnasium-Compatible RL Platform for the RoboCup Small Size League Built on Official Simulation Infrastructure**

## Abstract (Draft)
We present grsim-rl, an open-source reinforcement learning platform for the
RoboCup Small Size League (SSL) built directly on grSim, the official SSL
simulator. Unlike previous RL approaches to SSL that use custom physics
backends, grsim-rl preserves full compatibility with the official SSL
simulation protocol, game controller, and vision system while adding
Gymnasium and PettingZoo interfaces for single- and multi-agent RL research.
The platform provides hierarchical action spaces (wheel, body velocity, skill,
coach), a canonical event detection layer aligned with SSL rules, ten
curriculum-ready scenarios, and deterministic reset/replay capabilities.
We benchmark throughput, rule compliance, and baseline agent performance
across all scenarios. grsim-rl enables reproducible SSL research at the
intersection of robotics, multi-agent systems, and reinforcement learning.

## 1. Introduction
- RoboCup SSL as a multi-agent robotics benchmark
- Gap between team AI systems and RL research
- Need for standardized RL environments with official protocol compatibility
- Contributions: architecture, scenarios, hierarchical actions, event detection

## 2. Related Work
- rSoccer (Martins et al.): Custom physics, limited scenarios
- Google Research Football: Similar concept in soccer domain
- MuJoCo multi-agent environments
- PettingZoo ecosystem
- SSL team architectures (STP pattern)
- Official SSL simulation protocol

## 3. Architecture
- 3.1 grsim_core: ODE physics, headless stepping, state management
- 3.2 grsim_ref: Event detection, reward compilation
- 3.3 grsim_scenarios: Scenario system, hierarchical actions
- 3.4 pygrsim: Python bindings, Gymnasium/PettingZoo environments
- Figure: Architecture diagram with layer boundaries
- Figure: Data flow comparison (real-time vs training mode)

## 4. Scenario Design
- Table: All 10 scenarios with robot counts, time limits, action levels
- Scenario generation with seeded randomization
- Curriculum-ready difficulty progression
- Set-piece and full-play modes

## 5. Hierarchical Action Interfaces
- Four levels: wheel, body velocity, skill, coach
- Skill decomposition with debug traces
- Coach-level strategic abstraction
- Compatibility with STP, FSM, behavior tree architectures

## 6. Rule-Aligned Event Detection
- Canonical events mapped to ssl-rules sections
- Modular detector architecture (one per rule family)
- Separation of events from reward shaping
- Comparison with TIGERs AutoReferee coverage

## 7. Experiments
- 7.1 Throughput: steps/second across scenarios (headless vs real-time)
- 7.2 Rule compliance: event detection correctness on autoref-test scenarios
- 7.3 Baseline agents: PPO, SAC on single-agent scenarios
- 7.4 Multi-agent: MAPPO on 3v3 and 6v6 scenarios
- 7.5 Hierarchical comparison: wheel vs body vs skill vs coach performance
- 7.6 Reproducibility: cross-seed variance analysis

## 8. Discussion
- Comparison with rSoccer
- Protocol compatibility benefits
- Limitations: physics fidelity, compute requirements
- Future: sim-to-real transfer, curriculum learning

## 9. Conclusion

## Figures and Tables
1. Architecture diagram
2. Scenario table
3. Action hierarchy diagram
4. Event detector coverage table
5. Throughput benchmark bar chart
6. Learning curves for baseline agents
7. Multi-agent coordination metrics
8. Hierarchical action level comparison

## Venue
RoboCup Symposium (primary)
AAMAS (secondary)
