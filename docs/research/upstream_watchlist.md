# Upstream Watchlist

## Watched Repositories

| Repository | Watch Priority | Impact Areas |
|-----------|---------------|--------------|
| RoboCup-SSL/ssl-rules | Critical | Rule semantics, field dims, fouls, set pieces |
| RoboCup-SSL/ssl-game-controller | Critical | Game state, referee commands, CI mode |
| RoboCup-SSL/ssl-simulation-protocol | Critical | Simulator interop, control protocol |
| RoboCup-SSL/ssl-vision | High | Vision protocol, tracked state format |
| RoboCup-SSL/grSim | High | Upstream simulator changes, physics model |
| RoboCup-SSL/technical-challenge-rules | Medium | Benchmark scenarios |
| RoboCup-SSL/ssl-autoref-tests | Medium | Event detection test cases |
| RoboCup-SSL/ssl-simulation-setup | Low | Setup scripts, Docker configs |
| RoboCup-SSL/ssl-go-tools | Low | Reference tooling patterns |

## Change Classification

### Rules Changes
- Field dimension updates → Update FieldConfig defaults + tests
- Robot spec changes → Update RobotConfig defaults
- New fouls or modified fouls → Update event detectors + reward compiler
- Set piece rule changes → Update scenario definitions

### Protocol Changes  
- Simulation protocol updates → Update proto files + control adapters
- Vision protocol changes → Update vision output layer
- Game-controller API changes → Update CI integration adapters
- New robot control modes → Extend action interfaces

### Event Semantic Changes
- New game events → Add detectors
- Modified event conditions → Update detector logic + tests
- Event attribution changes → Update event struct fields

### Scenario/Benchmark Changes
- New technical challenges → Add benchmark scenarios
- Modified autoref tests → Update test fixtures
- New competition formats → Add scenario variants

## Impact Assessment Template

```markdown
## Upstream Change Impact Report

**Repository**: [repo name]
**Commit/PR**: [link]
**Date Detected**: [date]
**Classification**: [rules|protocols|events|scenarios|compatibility]

### Summary
[Brief description of change]

### Impact on grsim-rl
- [ ] Config defaults need update
- [ ] Proto files need sync
- [ ] Event detectors need update
- [ ] Scenarios need update
- [ ] Tests need update
- [ ] Documentation needs update
- [ ] Breaking change for users

### Proposed Actions
1. [action 1]
2. [action 2]

### Priority
[critical|high|medium|low]
```

## Polling Schedule

- **Critical repos**: Check weekly
- **High priority repos**: Check bi-weekly
- **Medium/Low repos**: Check monthly
- **Before any release**: Check all repos
