#!/bin/bash
# =============================================================
# run_experiments.sh
# Runs all 72 experiments: 4 D configs x 3 traces x 6 replications
# Output: results.csv  (used by analyze.py for two-factor analysis)
#
# Usage:
#   1. Edit TRACE_DIR below to the folder containing your decompressed traces.
#   2. Make sure the project is compiled:  make proj
#   3. Run:  bash run_experiments.sh
# =============================================================

PROJ=./proj
TRACE_DIR=.          # <-- UPDATE this path to where your trace files live
INST_COUNT=1000000   # 1 million instructions per replication

TRACES=("compute_fp_1" "compute_int_0" "srv_0")
D_VALUES=(1 2 3 4)
# Replication start instructions (1-indexed)
STARTS=(1 5000000 10000000 15000000 20000000 25000000)

# ---- Sanity checks ----
if [ ! -f "$PROJ" ]; then
    echo "ERROR: '$PROJ' not found. Run 'make proj' first."
    exit 1
fi

for trace in "${TRACES[@]}"; do
    if [ ! -f "$TRACE_DIR/$trace" ]; then
        echo "ERROR: trace file '$TRACE_DIR/$trace' not found."
        echo "       Decompress it with:  gunzip ${trace}.gz"
        exit 1
    fi
done

# ---- Write CSV header ----
OUTPUT=results.csv
echo "trace,D,replication,start_inst,total_cycles,exec_time_ms,pct_int,pct_fp,pct_branch,pct_load,pct_store" > "$OUTPUT"

total_runs=$(( ${#TRACES[@]} * ${#D_VALUES[@]} * ${#STARTS[@]} ))
run_num=0

for trace in "${TRACES[@]}"; do
    for D in "${D_VALUES[@]}"; do
        rep=1
        for start in "${STARTS[@]}"; do
            run_num=$((run_num + 1))
            echo "[$run_num/$total_runs] trace=$trace  D=$D  rep=$rep  start=$start"

            # Run simulator and capture output
            output=$("$PROJ" "$TRACE_DIR/$trace" "$start" "$INST_COUNT" "$D" 2>/dev/null)

            if [ $? -ne 0 ] || [ -z "$output" ]; then
                echo "  WARNING: simulation failed — writing empty row"
                echo "$trace,$D,$rep,$start,,,,,,,," >> "$OUTPUT"
                rep=$((rep + 1))
                continue
            fi

            # Parse each metric from printStats() output
            cycles=$(echo   "$output" | grep "Total Cycles:"          | awk '{print $3}')
            exec_t=$(echo   "$output" | grep "Execution Time"         | awk '{print $5}')
            pct_int=$(echo  "$output" | grep "^%Int:"                 | awk '{print $2}' | tr -d '%')
            pct_fp=$(echo   "$output" | grep "^%FP:"                  | awk '{print $2}' | tr -d '%')
            pct_br=$(echo   "$output" | grep "^%Branch:"              | awk '{print $2}' | tr -d '%')
            pct_ld=$(echo   "$output" | grep "^%Load:"                | awk '{print $2}' | tr -d '%')
            pct_st=$(echo   "$output" | grep "^%Store:"               | awk '{print $2}' | tr -d '%')

            echo "$trace,$D,$rep,$start,$cycles,$exec_t,$pct_int,$pct_fp,$pct_br,$pct_ld,$pct_st" >> "$OUTPUT"
            rep=$((rep + 1))
        done
    done
done

echo ""
echo "All $total_runs experiments complete."
echo "Results saved to: $OUTPUT"
echo "Now run:  python3 analyze.py"
