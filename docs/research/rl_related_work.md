# RL Related Work for SSL

## Existing RL Environments

### rSoccer (robocin/rSoccer)
**The closest existing work to grsim-rl.**

- Gymnasium-based environments using RSimSSL C++ backend
- 40 Hz timestep (0.025s)
- Normalized observation/action spaces [-1.2, 1.2]

**Scenarios**:
- Static Defenders (SSLStaticDefenders-v0): 1v1 obstacle course
- Contested Possession (SSLContestedPossession-v0): 1v1, 30s episodes
- Dribbling (SSLHWDribblingEnv): Zigzag obstacle field, 2min episodes
- Pass Endurance (SSLPassEndurance-v0): 2-robot passing, 30s episodes

**Observation Space**: Ball state (4D), robot state (7D per agent)
**Action Space**: Continuous Box (3-5D per agent: velocity + kick + dribbler)
**Rewards**: Scenario-specific (goal=1.0, checkpoint=1.0, distance shaping)

**Limitations**:
- Uses custom RSimSSL backend (not grSim)
- No official SSL protocol compatibility
- No referee/event integration
- Limited scenario set
- No multi-agent (PettingZoo) support
- No hierarchical actions
- No curriculum support

### Gaps grsim-rl Fills
1. **Official grSim physics** — same simulator used in competition
2. **SSL protocol compatibility** — works with real team software
3. **Referee integration** — canonical event detection + game-controller CI
4. **Hierarchical actions** — wheel → body → skill → coach
5. **Multi-agent** — PettingZoo for cooperative/competitive MARL
6. **Scenario richness** — 10+ scenarios covering rules and challenges
7. **Benchmark suite** — reproducible, publication-ready benchmarks
8. **Upstream tracking** — stays current with SSL rule changes

## RL Benchmarking in Multi-Agent Robotics

### Relevant Frameworks
- **Gymnasium** (single-agent): Step/reset API, observation/action spaces
- **PettingZoo** (multi-agent): Parallel/AEC APIs, agent selection
- **MARLlib**: Multi-agent RL library with common algorithms
- **EPyMARL**: Extended PyMARL for cooperative MARL

### Relevant Algorithms
- PPO, SAC for single-agent (skill learning)
- MAPPO, QMIX, MADDPG for multi-agent (team coordination)
- Hierarchical RL (options framework) for skill → coach decomposition
- Self-play for competitive training (blue vs yellow)

### Publication Positioning
- **RoboCup Symposium**: Primary venue (annual, peer-reviewed)
- **AAMAS**: Multi-agent systems
- **CoRL**: Robot learning
- **Key differentiator**: First RL platform built on official SSL simulator with full rule integration
