# CMPT 305 — Simple Processor Pipeline Simulator

Cycle-accurate simulator for a **2-wide superscalar**, **in-order** five-stage pipeline (IF → ID → EX → MEM → WB), as specified for the CMPT 305 course project. Implementation is in C++.

**Course:** CMPT 305 (SFU)  
**Assignment:** Simple Processor Pipeline Simulator — see Canvas for due date and submission details.

## Processor model

- **Stages:** Instruction Fetch (IF), Instruction Decode / read operands (ID), Issue and Execute (EX), Memory (MEM), Writeback / retire (WB).
- **Width:** Two instructions per cycle may advance when dependences allow.
- **Functional units (one of each):** integer ALU, floating-point unit, branch unit, one L1 data cache read port, one L1 data cache write port.

### Assumptions

1. No branch prediction.
2. All instruction fetches hit the L1 I-cache (no memory latency for fetch).
3. All loads and stores hit the L1 D-cache (no off-chip memory).
4. Integer and floating-point ops complete in one EX cycle in the baseline (see **D** configurations below).
5. Branches stall fetch until the branch **finishes EX**; the next instruction(s) enter IF in the **following** cycle.
6. Baseline loads and stores use the D-cache in one MEM cycle (see **D** for extended load MEM).

### Hazards modeled

1. **Structural:** No two integer (or two FP, or two branch) instructions in EX in the same cycle; no two loads (or two stores) in MEM in the same cycle.
2. **Control:** Fetch blocked until the branch completes EX.
3. **Data:** An instruction cannot enter EX until its dependences are satisfied. Dependence on int/FP is satisfied after that producer **finishes EX**; dependence on load/store is satisfied after the producer **finishes MEM**.

## Pipeline depth configuration `D`

| `D` | EX (FP) | MEM (load) | Frequency |
|-----|---------|------------|-----------|
| 1 | 1 cycle | 1 cycle | 1.0 GHz |
| 2 | 2 cycles (EX1, EX2) | 1 cycle | 1.2 GHz |
| 3 | 1 cycle | 3 cycles (MEM1–MEM3) | 1.7 GHz |
| 4 | 2 cycles (FP) | 3 cycles (load) | 1.8 GHz |

In-order execution is preserved: later instructions cannot pass earlier ones through the stretched EX or MEM regions.

**Execution time (ms)** = `(total simulation cycles) × (1 / frequency in Hz) × 1000`.

## Input trace format

Each line is one dynamic instruction, comma-separated:

1. **PC** — hexadecimal program counter.
2. **Type** — `1` integer, `2` floating-point, `3` branch, `4` load, `5` store.
3. **Dependences** — zero or more PCs of instructions this instruction depends on. Resolution uses the **last dynamic instance** of each static PC in the trace. For replications that do not start at instruction 1, dependences on instructions **before** the replication window are **ignored**.

Sample traces (~30M instructions each) are provided on Canvas (e.g. `compute_fp_1`, `compute_int_0`, `srv_0`); decompress with `gunzip` before use.

## Building and running

Submission expects a **Makefile** that builds an executable named `proj` on CSIL Linux. Typical invocation:

```text
./proj <trace_file_name> <start_inst> <inst_count> <D>
```

| Argument | Meaning |
|----------|---------|
| `trace_file_name` | Path to the decompressed trace file |
| `start_inst` | 1-based index of first instruction to simulate |
| `inst_count` | Number of instructions to simulate from that start |
| `D` | Pipeline depth: `1`, `2`, `3`, or `4` |

**Example** (matches course grading command):

```text
./proj srv_0 10000000 1000000 2
```

Simulates 1M instructions from the 10M-th instruction onward on trace `srv_0` with depth configuration 2.

If no Makefile is present yet, compile however your environment requires (e.g. produce `proj` from the project sources) so the above command line works.

## Output / metrics

Per run, the simulator reports (as required by the project):

- **Simulation clock** — cycles from start until the **last** of the `inst_count` instructions completes WB (retires).
- **Total execution time** — milliseconds, using the cycle count and the frequency for the chosen `D`.
- **Histogram** — fraction (or percentage) of retired instructions by type: int, FP, branch, load, store.

## Experimental design (course requirement)

- **Factors:** pipeline depth `D` ∈ {1,2,3,4} and trace `T` (three traces).
- **Replications:** six per configuration, each **1M** instructions, starting at instructions **1, 5M, 10M, 15M, 20M, 25M** (1-based indices).
- **Total runs:** 4 × 3 × 6 = **72** experiments.
- The written report must include tables (execution time in ms; %int, %FP, %branch, %load, %store), all replication results, two-factor analysis (means, level effects, variation allocation), and **per-member contributions**.

## Submission (Canvas)

Tarball `proj.tar.gz` should contain source, headers, Makefile, and the report PDF — **do not** bundle trace files. Graders extract, run `make proj`, then:

```text
./proj srv_0 10000000 1000000 2
```

## References

- Course materials (e.g. March 17 and March 24 lectures on pipelines).
- Traces derived from simplified [CVP1](https://www.microarch.org/cvp1/) style workloads.
