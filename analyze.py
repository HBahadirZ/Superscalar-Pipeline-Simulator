"""
analyze.py — Two-Factor Analysis for CMPT 305 Pipeline Simulator Project
=========================================================================
Reads results.csv produced by run_experiments.sh and computes:
  1. Full results table (all 72 rows)
  2. Overall mean execution time
  3. Impact (main effect) of each D level
  4. Impact (main effect) of each trace (T) level
  5. Allocation of variation: %D, %T, %D×T interaction, %Error

Run with:  python3 analyze.py
Requires:  pip install pandas numpy
"""

import sys
import numpy as np
import pandas as pd

CSV_FILE = "results.csv"

# ── Load data ──────────────────────────────────────────────────────────────
try:
    df = pd.read_csv(CSV_FILE)
except FileNotFoundError:
    print(f"ERROR: '{CSV_FILE}' not found. Run run_experiments.sh first.")
    sys.exit(1)

required = {"trace", "D", "replication", "exec_time_ms",
            "pct_int", "pct_fp", "pct_branch", "pct_load", "pct_store",
            "total_cycles"}
missing = required - set(df.columns)
if missing:
    print(f"ERROR: CSV is missing columns: {missing}")
    sys.exit(1)

df["exec_time_ms"] = pd.to_numeric(df["exec_time_ms"], errors="coerce")
df = df.dropna(subset=["exec_time_ms"])

# Fixed ordering
D_LEVELS = [1, 2, 3, 4]
T_LEVELS  = sorted(df["trace"].unique())   # alphabetical
a = len(D_LEVELS)   # 4  (number of D levels)
b = len(T_LEVELS)   # 3  (number of traces)
n = 6               # replications per cell

# ── Full results table ─────────────────────────────────────────────────────
print("=" * 90)
print("FULL RESULTS TABLE (all 72 experiments)")
print("=" * 90)
header = f"{'Trace':<18} {'D':>2} {'Rep':>4} {'Cycles':>12} {'Time(ms)':>12} " \
         f"{'%Int':>6} {'%FP':>6} {'%Br':>6} {'%Ld':>6} {'%St':>6}"
print(header)
print("-" * 90)
for trace in T_LEVELS:
    for D in D_LEVELS:
        subset = df[(df["trace"] == trace) & (df["D"] == D)].sort_values("replication")
        for _, row in subset.iterrows():
            print(f"{row['trace']:<18} {int(row['D']):>2} {int(row['replication']):>4} "
                  f"{int(row['total_cycles']):>12} {row['exec_time_ms']:>12.6f} "
                  f"{row['pct_int']:>6.2f} {row['pct_fp']:>6.2f} "
                  f"{row['pct_branch']:>6.2f} {row['pct_load']:>6.2f} "
                  f"{row['pct_store']:>6.2f}")
        print()

# ── Build 3-D array  y[i, j, k]  (D × T × replication) ───────────────────
y = np.zeros((a, b, n))
for i, D in enumerate(D_LEVELS):
    for j, T in enumerate(T_LEVELS):
        vals = (df[(df["D"] == D) & (df["trace"] == T)]
                .sort_values("replication")["exec_time_ms"]
                .values)
        if len(vals) != n:
            print(f"WARNING: expected {n} replications for D={D}, trace={T}, "
                  f"got {len(vals)}. Results may be inaccurate.")
        y[i, j, :len(vals)] = vals

# ── Means ──────────────────────────────────────────────────────────────────
grand_mean  = np.mean(y)                       # scalar
d_means     = np.mean(y, axis=(1, 2))          # shape (a,)  — averaged over T and reps
t_means     = np.mean(y, axis=(0, 2))          # shape (b,)  — averaged over D and reps
cell_means  = np.mean(y, axis=2)               # shape (a, b)

# ── Sums of squares ────────────────────────────────────────────────────────
SS_D        = n * b * np.sum((d_means - grand_mean) ** 2)
SS_T        = n * a * np.sum((t_means - grand_mean) ** 2)
SS_DT       = n * np.sum(
                  (cell_means
                   - d_means[:, None]
                   - t_means[None, :]
                   + grand_mean) ** 2
              )
SS_E        = np.sum((y - cell_means[:, :, None]) ** 2)
SS_Total    = np.sum((y - grand_mean) ** 2)

# ── 1. Overall mean ────────────────────────────────────────────────────────
print("=" * 60)
print("1. OVERALL MEAN EXECUTION TIME")
print("=" * 60)
print(f"   Grand mean: {grand_mean:.6f} ms\n")

# ── 2. Impact of each D level ──────────────────────────────────────────────
print("=" * 60)
print("2. IMPACT OF PIPELINE DEPTH (D)")
print("=" * 60)
print(f"  {'D':>4}  {'Mean (ms)':>12}  {'Effect (ms)':>12}")
print("  " + "-" * 34)
for i, D in enumerate(D_LEVELS):
    effect = d_means[i] - grand_mean
    print(f"  {D:>4}  {d_means[i]:>12.6f}  {effect:>+12.6f}")
print()

# ── 3. Impact of each trace (T) level ─────────────────────────────────────
print("=" * 60)
print("3. IMPACT OF TRACE (T)")
print("=" * 60)
print(f"  {'Trace':<18}  {'Mean (ms)':>12}  {'Effect (ms)':>12}")
print("  " + "-" * 48)
for j, T in enumerate(T_LEVELS):
    effect = t_means[j] - grand_mean
    print(f"  {T:<18}  {t_means[j]:>12.6f}  {effect:>+12.6f}")
print()

# ── 4. Cell means table ────────────────────────────────────────────────────
print("=" * 60)
print("4. CELL MEANS (ms)  [D × Trace]")
print("=" * 60)
header = f"  {'D':>4}" + "".join(f"  {T:>18}" for T in T_LEVELS)
print(header)
print("  " + "-" * (6 + 20 * b))
for i, D in enumerate(D_LEVELS):
    row_str = f"  {D:>4}"
    for j in range(b):
        row_str += f"  {cell_means[i, j]:>18.6f}"
    print(row_str)
print()

# ── 5. Allocation of variation ─────────────────────────────────────────────
print("=" * 60)
print("5. ALLOCATION OF VARIATION")
print("=" * 60)
print(f"  {'Source':<20}  {'SS':>14}  {'%':>8}")
print("  " + "-" * 48)
sources = [
    ("D (pipeline depth)", SS_D),
    ("T (trace)",          SS_T),
    ("D×T (interaction)",  SS_DT),
    ("Error",              SS_E),
    ("Total",              SS_Total),
]
for name, SS in sources:
    pct = 100.0 * SS / SS_Total if SS_Total > 0 else 0.0
    print(f"  {name:<20}  {SS:>14.6f}  {pct:>7.2f}%")
print()

# ── Summary interpretation ─────────────────────────────────────────────────
print("=" * 60)
print("INTERPRETATION SUMMARY")
print("=" * 60)
dominant = max(sources[:-1], key=lambda x: x[1])
print(f"  Largest source of variation: {dominant[0]}")
print(f"    ({100.0 * dominant[1] / SS_Total:.1f}% of total variation)")
print()
print("  D effects (positive = slower than average):")
for i, D in enumerate(D_LEVELS):
    tag = "<-- fastest" if d_means[i] == d_means.min() else \
          "<-- slowest" if d_means[i] == d_means.max() else ""
    print(f"    D{D}: {d_means[i]:>10.6f} ms  (effect {d_means[i]-grand_mean:>+.6f} ms)  {tag}")
print()
print("  Trace effects:")
for j, T in enumerate(T_LEVELS):
    tag = "<-- fastest" if t_means[j] == t_means.min() else \
          "<-- slowest" if t_means[j] == t_means.max() else ""
    print(f"    {T:<18}: {t_means[j]:>10.6f} ms  (effect {t_means[j]-grand_mean:>+.6f} ms)  {tag}")
