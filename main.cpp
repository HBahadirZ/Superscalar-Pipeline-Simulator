// CMPT 305 Project main source file

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <unordered_map>
using namespace std;

// SHARED: Instruction
struct Instruction {
    unsigned long long pc; // instruction address (hex in trace)
    int type; // 1:Int 2:FP 3:Branch 4:Load 5:Store
    vector<unsigned long long> dep_pcs; // PCs this instruction depends on (trace)

    // Current pipeline stage (driven by simulation loop — Won)
    // 0:IF 1:ID 2:EX 3:MEM 4:WB 5:Retired
    int pipeline_stage= 0;

    // Cycle when instruction enters each stage (-1 means not yet reached)
    long long cycle_IF = -1;
    long long cycle_ID = -1;
    long long cycle_EX = -1;
    long long cycle_MEM = -1;
    long long cycle_WB = -1;

    // Cycle when EX and MEM stages ends
    // instructions in general; cycle_EX_done = cycle_EX (1-cycle EX)
    // cycle_MEM_done = cycle_MEM (1-cycle MEM)
    // D2/D4 FP instructions; cycle_EX_done = cycle_EX + 1 (2-cycle EX)
    // D3/D4 Load instructions; cycle_MEM_done = cycle_MEM + 2 (3-cycle MEM)
    long long cycle_EX_done = -1;
    long long cycle_MEM_done = -1;
    long long cycle_WB_done = -1; // retire cycle;simulation end when last inst retires
};

// SHARED: pipelineConfiguration (to Encodes the D1/D2/D3/D4 pipeline depth parameters)
struct pipelineConfiguration {
    int D; // pipeline depth level: 1, 2, 3, or 4
    int EX_cycles_fp_inst;// EX stage cycles for FP instructions (D1/D3:1, D2/D4:2)
    int MEM_cycles_load;// MEM stage cycles for Load instructions (D1/D2:1, D3/D4:3)
    double frequency_in_ghz; // processor frequency (D1:1, D2:1.2, D3:1.7, D4:1.8)
};

// Return correct pipelineConfiguration for a given D val
// Huseyin
pipelineConfiguration makeConfig(int D) {
    // Todo: fill in all four cases
    pipelineConfiguration pipConfig;
    pipConfig.D = D;
    switch (D) {
        case 1: pipConfig.EX_cycles_fp_inst = 1; pipConfig.MEM_cycles_load = 1; pipConfig.frequency_in_ghz = 1.0; break;
        case 2: pipConfig.EX_cycles_fp_inst = 2; pipConfig.MEM_cycles_load = 1; pipConfig.frequency_in_ghz = 1.2; break;
        case 3: pipConfig.EX_cycles_fp_inst = 1; pipConfig.MEM_cycles_load = 3; pipConfig.frequency_in_ghz = 1.7; break;
        case 4: pipConfig.EX_cycles_fp_inst = 2; pipConfig.MEM_cycles_load = 3; pipConfig.frequency_in_ghz = 1.8; break;
        default: cerr << "It is invalid D value: " << D << endl; exit(1);
    }
    return pipConfig;
}

// SHARED: SimulationResult
// Returned by Simulator::run() printed by printStats()
// Huseyin
struct SimulationResult{
    long long totalCycles = 0;
    double execution_time_in_ms = 0;

    // Retired instruction counts by different types
    long long count_int= 0;
    long long count_fp = 0;
    long long count_branch = 0;
    long long count_load = 0;
    long long count_store = 0;
};

// Printing simulation stats to StdOut
// Huseyin
void printStats(const SimulationResult& r) {
    long long total = r.count_int + r.count_fp + r.count_branch + r.count_load + r.count_store;
    cout << "Total Cycles: " << r.totalCycles << endl;
    cout << "Execution Time (in ms): " <<r.execution_time_in_ms<< endl;
    cout << "Instruction Histogram: " << endl;


    // Todo: printing percentages for each type
    cout << "%Int: " << (100.0 * r.count_int / total) << "%" << endl;
    cout << "%FP: " << (100.0 * r.count_fp / total) << "%" << endl;
    cout << "%Branch: " << (100.0 * r.count_branch / total) << "%" << endl;
    cout << "%Load: " << (100.0 * r.count_load / total) << "%" << endl;
    cout << "%Store: " << (100.0 * r.count_store / total) << "%" << endl;
}

// TraceReader to parse the instruction trace file one instruction at a time
// Seeks to start_instruction on construction
// Won
class TraceReader {
public:
    // Opens trace file and advances to start_instruction (1-indexed)
    // start_instruction=1 means start from the very first instruction
    TraceReader(const string& path, long long start_instruction) {
        // Todo:open file, skip (start_instruction-1) lines
    }

    ~TraceReader() {
        // Todo:close file if open
    }

    // Returns true if there are more instructions to read
    bool hasNext() const {
        // Todo
        return false;
    }

    // Parses and returns the next instruction from the trace
    Instruction next() {
        // Todo
        return Instruction{};
    }

private:
    ifstream file;
    bool EOF_reached = false;
};

// ============================================================
// Simulator
// together
//
// Won:
// - pipeline loop (advanceStages)
// - structural hazard detection (functional unit conflicts)
// - control hazard detection (branch fetch stall)
// - D1 baseline simulation
//
// Huseyin:
// - data hazard detection (dep_ready map lookups)
// - extended EX/MEM stage logic for D2/D3/D4
// - result tallying and execution time calculation
// ============================================================
class Simulator {
public:
    Simulator(const pipelineConfiguration& config) : config(config) {}

    // Simulate exactly instruction_count instructions from reader
    // Then return performance metrics for this run
    SimulationResult run(TraceReader& reader, long long instruction_count) {
        SimulationResult result;
        long long cycle=0;

        // Active instructions in the pipeline
        deque<Instruction*> pipeline;

        // Dependency map: PC -> cycle when that instruction's
        // data dependency becomes satisfied
        // INT/FP/Branch: satisfied after EX (cycle_EX_done)
        // Load/Store: satisfied after MEM (cycle_MEM_done)
        // Updated by Huseyin each time an instruction retires a stage
        dep_ready.clear();

        long long retired_instruction = 0;

        // Todo: main simulation loop
        // Won
        // Structure:
        // while(retired < instruction_count):
        // 1.Fetch up to 2 new instructions (if no branch stall)
        // 2.Advance pipeline: try to move each instruction forward
        // -Check structural hazard before EX
        // -Check control hazard before IF
        // -Check data hazard before EX(Huseyin)
        // -Check extended EX/MEM cycles(Huseyin)
        // 3.Retire instructions that finished WB then update dep_ready
        // 4.Increment cycle


        // Just placeholders (remove later)
        (void)reader;
        (void)instruction_count;
        (void)cycle;
        (void)pipeline;
        (void)retired_instruction;

        return result;
    }

private:
    pipelineConfiguration config;


    // Dependency map (see comments in run())
    // Key: instruction PC
    // value: earliest cycle at which a dependent instruction may enter EX
    unordered_map<unsigned long long, long long> dep_ready;
};

// Main Function
// Huseyin
// Parses CLI arguments and runs one simulation
// Running all 72 experiments for grading
// How To Compile: make proj
// How To Run: ./proj trace_file start_instruction instruction_count D
// Example Usage: ./proj srv_0 10000000 1000000 2
int main(int argc,char* argv[]) {
    if (argc!=5) {
        cerr << "Wrong usage!" << endl;
        return 1;
    }

    string trace_path = argv[1];
    long long start_instruction = stoll(argv[2]); // 1-indexed instruction to start from
    long long instruction_count = stoll(argv[3]); // number of instructions to simulate
    int D= stoi(argv[4]); // pipeline depth config: 1,2,3, or 4

    pipelineConfiguration config = makeConfig(D);
    TraceReader reader(trace_path,start_instruction);
    Simulator sim(config);
    SimulationResult result = sim.run(reader, instruction_count);
    printStats(result);

    return 0;
}
