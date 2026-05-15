# Test Suite

This directory contains automated tests for the pipeline simulator.

## Run Tests

From the repository root:

```bash
python tests/run_test_suite.py
```

## What Is Covered

- **Functional behavior**: expected metrics are emitted and histogram totals are valid.
- **Configuration effects**: latency/frequency differences across `D=1..4` behave as expected.
- **CLI validation**: invalid argument count and invalid `D` are rejected.
- **Robustness checks**: missing traces, invalid `start_inst`, and trace exhaustion return failures.

## Generated Test Data

Tests generate temporary trace files under `tests/generated_traces/` at runtime.
These files are treated as generated artifacts and are ignored by `.gitignore`.
