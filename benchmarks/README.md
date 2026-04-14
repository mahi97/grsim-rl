# Benchmarks

## Benchmark Suite

### Throughput Benchmarks
Measure simulation steps per second in headless mode:

```bash
# When C++ core is built:
./build/bench_throughput --scenario empty_field_shot --steps 10000
./build/bench_throughput --scenario mini_game_full --steps 10000

# Python environment overhead:
python bench_env_overhead.py
```

### Event Detection Correctness
Test canonical events against ssl-autoref-tests:

```bash
python bench_event_correctness.py
```

### Baseline Agent Performance
Train and evaluate PPO/SAC baselines:

```bash
python bench_baselines.py --algo ppo --scenario empty_field_shot --timesteps 1000000
python bench_baselines.py --algo sac --scenario 1v1_dribble --timesteps 1000000
```

### Reproducibility
Run identical seeds and verify determinism:

```bash
python bench_reproducibility.py --seeds 10 --steps 1000
```

## Expected Results

| Metric | Old grSim (real-time) | grsim_core (headless) | Ratio |
|--------|----------------------|----------------------|-------|
| Steps/sec (1 robot) | ~60 (wall clock) | ~5,000+ | ~80x |
| Steps/sec (6v6) | ~60 (wall clock) | ~1,000+ | ~16x |
| Steps/sec (11v11) | ~60 (wall clock) | ~500+ | ~8x |

| Scenario | PPO Success Rate | SAC Success Rate |
|----------|-----------------|-----------------|
| empty_field_shot | TBD | TBD |
| 1v1_dribble | TBD | TBD |
| goalkeeper_save | TBD | TBD |
| ball_placement | TBD | TBD |

## Running All Benchmarks

```bash
python run_all_benchmarks.py --output-dir ../artifacts/benchmarks/
```
