// CMPT 305 Project main source file

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <unordered_map>
#include <climits>
#include <algorithm>
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
    cout << "Execution Time (in ms): " << r.execution_time_in_ms << endl;
    cout << "Instruction Histogram: " << endl;
    if (total == 0) {
        cout << "%Int: 0%" << endl;
        cout << "%FP: 0%" << endl;
        cout << "%Branch: 0%" << endl;
        cout << "%Load: 0%" << endl;
        cout << "%Store: 0%" << endl;
        return;
    }
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
        dep_ready.clear();

        if (instruction_count <= 0) {
            result.totalCycles = 0;
            result.execution_time_in_ms = 0;
            return result;
        }

        deque<Instruction*> q;
        long long retired = 0;
        long long fetched = 0;
        long long cycle = 0;
        // Loose cap so we never spin forever if TraceReader is empty or logic regresses
        const long long cycle_cap = instruction_count * 25LL + 10000000LL;

        while (retired < instruction_count) {
            if (cycle > cycle_cap) {
                cerr << "Simulator: cycle limit exceeded (check TraceReader / hazards)\n";
                break;
            }

            // --- control hazard: stall fetch while any branch has not finished EX ---
            bool fetch_stall = false;
            for (Instruction* x : q) {
                if (x->type != 3) continue;
                if (x->pipeline_stage < 2) {
                    fetch_stall = true;
                    break;
                }
                if (x->pipeline_stage == 2 && cycle <= x->cycle_EX_done) {
                    fetch_stall = true;
                    break;
                }
            }

            // --- retire WB (oldest at front); PDF: Nth completes WB ---
            while (!q.empty() && q.front()->pipeline_stage == 4 && cycle > q.front()->cycle_WB) {
                Instruction* r = q.front();
                q.pop_front();
                switch (r->type) {
                    case 1: result.count_int++; break;
                    case 2: result.count_fp++; break;
                    case 3: result.count_branch++; break;
                    case 4: result.count_load++; break;
                    case 5: result.count_store++; break;
                    default: break;
                }
                delete r;
                retired++;
                if (retired >= instruction_count) break;
            }
            if (retired >= instruction_count) {
                cycle++;
                break;
            }

            // --- MEM -> WB (max 2), in-order: any older inst still in MEM blocks ---
            int moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 3) continue;
                if (cycle <= in->cycle_MEM_done) continue;
                bool blocked = false;
                for (size_t j = 0; j < i; ++j) {
                    if (q[j]->pipeline_stage == 3) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;
                in->pipeline_stage = 4;
                in->cycle_WB = cycle;
                in->cycle_WB_done = cycle;
                moved++;
            }

            // --- EX -> MEM (max 2), structural load/store ports ---
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 2) continue;
                if (cycle <= in->cycle_EX_done) continue;
                bool blocked = false;
                for (size_t j = 0; j < i; ++j) {
                    if (q[j]->pipeline_stage == 2) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;

                auto load_port_busy = [&]() {
                    for (Instruction* x : q)
                        if (x->type == 4 && x->pipeline_stage == 3 && cycle <= x->cycle_MEM_done)
                            return true;
                    return false;
                };
                auto store_port_busy = [&]() {
                    for (Instruction* x : q)
                        if (x->type == 5 && x->pipeline_stage == 3 && cycle <= x->cycle_MEM_done)
                            return true;
                    return false;
                };
                if (in->type == 4 && load_port_busy()) continue;
                if (in->type == 5 && store_port_busy()) continue;

                int mem_lat = (in->type == 4) ? config.MEM_cycles_load : 1;
                long long mem_done = cycle + mem_lat - 1;
                if (config.D == 3 || config.D == 4) {
                    for (size_t j = 0; j < i; ++j) {
                        Instruction* o = q[j];
                        if (o->pipeline_stage == 3 && o->type == 4 && in->type != 4)
                            mem_done = max(mem_done, o->cycle_MEM_done);
                    }
                }

                in->pipeline_stage = 3;
                in->cycle_MEM = cycle;
                in->cycle_MEM_done = mem_done;
                if (in->type == 4 || in->type == 5)
                    dep_ready[in->pc] = mem_done + 1;
                moved++;
            }

            // --- ID -> EX (max 2) ---
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 1) continue;
                if (cycle <= in->cycle_ID) continue;
                bool blocked = false;
                for (size_t j = 0; j < i; ++j) {
                    if (q[j]->pipeline_stage == 1) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;

                bool data_ok = true;
                for (unsigned long long dpc : in->dep_pcs) {
                    auto it = dep_ready.find(dpc);
                    if (it != dep_ready.end() && cycle < it->second) {
                        data_ok = false;
                        break;
                    }
                }
                if (!data_ok) continue;

                auto fu_busy = [&](int typ) {
                    for (Instruction* x : q) {
                        if (x->pipeline_stage != 2) continue;
                        if (x->type != typ) continue;
                        if (cycle <= x->cycle_EX_done) return true;
                    }
                    return false;
                };
                if (in->type == 1 && fu_busy(1)) continue;
                if (in->type == 2 && fu_busy(2)) continue;
                if (in->type == 3 && fu_busy(3)) continue;

                int ex_lat = (in->type == 2) ? config.EX_cycles_fp_inst : 1;
                long long ex_done = cycle + ex_lat - 1;
                if (config.D == 2 || config.D == 4) {
                    for (size_t j = 0; j < i; ++j) {
                        Instruction* o = q[j];
                        if (o->pipeline_stage == 2 && o->type == 2 && in->type != 2)
                            ex_done = max(ex_done, o->cycle_EX_done);
                    }
                }

                in->pipeline_stage = 2;
                in->cycle_EX = cycle;
                in->cycle_EX_done = ex_done;
                if (in->type <= 3)
                    dep_ready[in->pc] = ex_done + 1;
                else
                    dep_ready[in->pc] = LLONG_MAX;
                moved++;
            }

            // --- IF -> ID (max 2) ---
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 0) continue;
                if (cycle <= in->cycle_IF) continue;
                bool blocked = false;
                for (size_t j = 0; j < i; ++j) {
                    if (q[j]->pipeline_stage == 0) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;
                in->pipeline_stage = 1;
                in->cycle_ID = cycle;
                moved++;
            }

            // --- fetch ---
            if (!fetch_stall && fetched < instruction_count) {
                int fcnt = 0;
                while (fcnt < 2 && fetched < instruction_count && reader.hasNext()) {
                    Instruction* ni = new Instruction(reader.next());
                    ni->pipeline_stage = 0;
                    ni->cycle_IF = cycle;
                    q.push_back(ni);
                    fetched++;
                    fcnt++;
                    if (ni->type == 3) break;
                }
            }

            // Deadlock / empty trace: no progress possible
            if (q.empty() && retired < instruction_count && !reader.hasNext()) break;

            cycle++;
        }

        result.totalCycles = cycle;
        result.execution_time_in_ms =
            static_cast<double>(cycle) / (config.frequency_in_ghz * 1e6);
        // Clean up if we exited early
        while (!q.empty()) {
            delete q.front();
            q.pop_front();
        }
        return result;
    }

private:
    pipelineConfiguration config;

    // dep_ready[producer_pc] = earliest cycle a dependent may enter EX
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
