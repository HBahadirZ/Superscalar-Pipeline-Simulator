# Superscalar Pipeline Simulator (C++)

A cycle-accurate simulator for a **2-wide, in-order, 5-stage pipeline** (`IF -> ID -> EX -> MEM -> WB`).
It models structural, control, and data hazards, then reports performance metrics from instruction traces.

This project was originally built for computer architecture coursework and has been cleaned up into a standalone personal systems project.

## Features

- 2-wide superscalar, in-order issue and retirement model.
- Instruction support: integer, floating-point, branch, load, store.
- Hazard modeling:
  - structural conflicts (execution units and memory ports)
  - control stalls from unresolved branches
  - data dependency blocking
- Multiple latency/frequency configurations via `D=1..4`.
- Metrics output:
  - total cycle count
  - execution time in milliseconds
  - retired instruction type histogram

## Quick Start

```bash
make
./proj tests/generated_traces/mixed.trace 1 5 2
python tests/run_test_suite.py
```

## Pipeline Configurations

| D | FP EX Latency | Load MEM Latency | Frequency |
|---|---------------|------------------|-----------|
| 1 | 1 cycle       | 1 cycle          | 1.0 GHz   |
| 2 | 2 cycles      | 1 cycle          | 1.2 GHz   |
| 3 | 1 cycle       | 3 cycles         | 1.7 GHz   |
| 4 | 2 cycles      | 3 cycles         | 1.8 GHz   |

Execution time:

`execution_time_ms = cycles / (frequency_GHz * 1e6)`

## Trace Input Format

Each trace line is:

`<pc_hex>,<type>[,<dep_pc_hex_1>,<dep_pc_hex_2>, ...]`

Type mapping:

- `1`: integer
- `2`: floating-point
- `3`: branch
- `4`: load
- `5`: store

Example:

```text
1000,1
1004,2,1000
1008,3,1004
100C,4
```

## Build and Run

Build from repo root:

```bash
make
```

Run:

```bash
./proj <trace_file> <start_inst> <inst_count> <D>
```

Arguments:

- `trace_file`: path to trace file
- `start_inst`: 1-based index of first instruction to simulate
- `inst_count`: number of instructions to simulate
- `D`: pipeline configuration (`1`, `2`, `3`, or `4`)

Example:

```bash
./proj tests/generated_traces/mixed.trace 1 5 2
```

## Sample Output

```text
Total Cycles: 13
Execution Time (in ms): 1.08333e-05
Instruction Histogram:
%Int: 20%
%FP: 20%
%Branch: 20%
%Load: 20%
%Store: 20%
```

## Testing

Run the test suite:

```bash
python tests/run_test_suite.py
```

The tests generate temporary traces under `tests/generated_traces/`.

## Project Structure

- `main.cpp` - simulator implementation
- `Makefile` - build target for `proj`
- `tests/run_test_suite.py` - automated tests
- `tests/README.md` - test details

## Platform Notes

- Development target is Linux toolchain (`make`, `g++`).
- On Windows, you can run build/tests through WSL.

## Repository Hygiene

- Large external workload traces are not committed.
- Build outputs and generated files are excluded via `.gitignore`.
