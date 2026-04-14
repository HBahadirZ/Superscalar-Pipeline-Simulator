// CMPT 305 Project main source file

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <unordered_map>
#include <climits>
#include <limits>
#include <algorithm>
using namespace std;

// Instruction struct to store the information of an instruction
struct Instruction {
    unsigned long long pc; // instruction address (hex in trace)
    int type; // 1:Int  2:FP  3:Branch  4:Load  5:Store
    vector<unsigned long long> dep_pcs; // PCs this instruction depends on (from trace)

    // Current pipeline stage (driven by simulation loop)
    // 0:IF 1:ID 2:EX 3:MEM 4:WB 5:Retired
    int pipeline_stage = 0;

    // Cycle when instruction enters each stage (-1 = not yet reached)
    long long cycle_IF  = -1;
    long long cycle_ID  = -1;
    long long cycle_EX  = -1;
    long long cycle_MEM = -1;
    long long cycle_WB  = -1;

    // Cycle when instruction finishes each stage (-1 = not yet finished)
    long long cycle_EX_done  = -1;
    long long cycle_MEM_done = -1;
    long long cycle_WB_done  = -1; // retire cycle; simulation ends when the last instruction retires
};

// pipelineConfiguration struct to store the configuration of the pipeline
struct pipelineConfiguration {
    int D; // pipeline depth level: 1, 2, 3, or 4
    int EX_cycles_fp_inst; // EX stage cycles for FP instructions (D1/D3:1, D2/D4:2)
    int MEM_cycles_load; // MEM stage cycles for Load instructions (D1/D2:1, D3/D4:3)
    double frequency_in_ghz; // processor frequency (D1:1.0 GHz, D2:1.2 GHz, D3:1.7 GHz, D4:1.8 GHz)
};

// Returns the correct pipelineConfiguration for a given D value.
pipelineConfiguration makeConfig(int D) {
    pipelineConfiguration pipConfig;
    pipConfig.D = D;
    switch (D) {
        case 1: pipConfig.EX_cycles_fp_inst = 1; pipConfig.MEM_cycles_load = 1; pipConfig.frequency_in_ghz = 1.0; break;
        case 2: pipConfig.EX_cycles_fp_inst = 2; pipConfig.MEM_cycles_load = 1; pipConfig.frequency_in_ghz = 1.2; break;
        case 3: pipConfig.EX_cycles_fp_inst = 1; pipConfig.MEM_cycles_load = 3; pipConfig.frequency_in_ghz = 1.7; break;
        case 4: pipConfig.EX_cycles_fp_inst = 2; pipConfig.MEM_cycles_load = 3; pipConfig.frequency_in_ghz = 1.8; break;
        default: cerr << "Invalid D value: " << D << endl; exit(1);
    }
    return pipConfig;
}

// SimulationResult struct to store the results of the simulation
struct SimulationResult {
    long long totalCycles = 0; // total cycles of the simulation
    double execution_time_in_ms = 0; // execution time of the simulation in milliseconds
    bool completed = false; // true only if the requested instruction count retired successfully

    // Retired instruction counts by different types: integer, floating-point, branch, load, store
    long long count_int= 0; 
    long long count_fp = 0;
    long long count_branch = 0;
    long long count_load = 0;
    long long count_store = 0;
};

// Prints simulation stats to stdout.
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
class TraceReader {
public:
    TraceReader(const string& path,long long start_instruction) {
        file.open(path);
        if (!file.is_open()) {
            cerr << "Error: cannot open trace file: " << path << endl;
            return;
        }
        // Skip the first (start_instruction -1) lines
        // Using ignore() to avoid allocating strings for skipped lines
        for (long long i = 1; i < start_instruction; ++i) {
            file.ignore(numeric_limits<streamsize>::max(), '\n');
            if (!file.good()) return; // reached EOF
        }
    }

    ~TraceReader() {
        if (file.is_open()) file.close();
    }

    // Returns true if there is at least one more instruction to read
    bool hasNext() {
        // peek() returns EOF only when the stream has no more char
        return file.is_open() && file.peek() != EOF;
    }

    bool isOpen() const {
        return file.is_open();
    }

    // Parses and returns the next instruction from the trace
    Instruction next() {
        Instruction inst;
        string line;

        // Skip blank lines and get the next line
        while (getline(file, line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (!line.empty()) break;
        }
        if (line.empty()) return inst; // EOF or only blank lines remain

        stringstream ss(line);
        string token;

        // Field1: instruction PC (hexadecimal)
        if (getline(ss, token, ','))
            inst.pc = stoull(token, nullptr, 16);

        // Field2: instruction type
        if (getline(ss, token, ','))
            inst.type = stoi(token);

        // Remaining fields: dependency PCs (hexadecimal), can be empty
        while (getline(ss, token,',')) {
            if (!token.empty())
                inst.dep_pcs.push_back(stoull(token, nullptr, 16));
        }

        return inst;
    }

private:
    ifstream file;
};

// Simulator: 2-wide superscalar in-order pipeline
// Simulates the pipeline and returns the results of the simulation
class Simulator {
public:
    Simulator(const pipelineConfiguration& config) : config(config) {}

    // Simulate exactly instruction_count instructions from reader
    // Then return the results of the simulation
    SimulationResult run(TraceReader& reader, long long instruction_count) {
        SimulationResult result;
        dep_ready.clear();

        if (instruction_count <= 0) {
            result.completed = true;
            return result;
        }

        deque<Instruction*> q;
        long long retired = 0;
        long long fetched = 0;
        long long cycle = 0;
        // Safety cap: prevents infinite loop if TraceReader or hazard logic regresses
        const long long cycle_cap = instruction_count * 25LL + 10000000LL;

        while (retired < instruction_count) {
            if (cycle > cycle_cap) {
                cerr << "Simulator: cycle limit exceeded (check TraceReader / hazards)\n";
                break;
            }

            // Control hazard: stall fetch while any branch hasn't finished EX
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

            // retire WB (oldest at front); Nth instruction completes WB
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

            // MEM -> WB (up to 2 per cycle, in-order)
            int moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 3) continue;
                if (cycle <= in->cycle_MEM_done) continue;
                bool blocked = false;
                for (size_t j = 0; j <i && !blocked; ++j)
                    if (q[j]->pipeline_stage == 3){
                        blocked = true;
                    }
                if (blocked){
                    continue;
                }
                in->pipeline_stage = 4;
                in->cycle_WB = cycle;
                in->cycle_WB_done = cycle;
                moved++;
            }

            // EX -> MEM (max 2), structural load/store ports
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 2) continue;
                if (cycle <= in->cycle_EX_done) continue;
                // In-order: no older instruction still in EX
                bool blocked = false;
                for (size_t j = 0; j < i && !blocked; ++j)
                    if (q[j]->pipeline_stage == 2){
                        blocked = true;
                    }
                if (blocked) continue;

                // Structural hazard: load/store port conflicts
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
                if (in->type == 4 && load_port_busy())  continue;
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
                // Data hazard resolution
                if (in->type == 4 || in->type == 5)
                    dep_ready[in->pc] = mem_done + 1;
                moved++;
            }

            // ID -> EX (max 2)
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 1) continue;
                if (cycle <= in->cycle_ID) continue;
                bool blocked = false;
                for (size_t j = 0; j < i && !blocked; ++j){
                    if (q[j]->pipeline_stage == 1){
                        blocked = true;
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
                    for (Instruction* x : q)
                        if (x->pipeline_stage == 2 && x->type == typ && cycle <= x->cycle_EX_done) return true;
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

            // IF -> ID (max 2)
            moved = 0;
            for (size_t i = 0; i < q.size() && moved < 2; ++i) {
                Instruction* in = q[i];
                if (in->pipeline_stage != 0) continue;
                if (cycle <= in->cycle_IF) continue;
                bool blocked = false;
                for (size_t j = 0; j < i && !blocked; ++j)
                    if (q[j]->pipeline_stage == 0) {
                        blocked = true;
                    }
                if (blocked) continue;
                in->pipeline_stage = 1;
                in->cycle_ID = cycle;
                moved++;
            }

            // fetch
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
        result.execution_time_in_ms = static_cast<double>(cycle) / (config.frequency_in_ghz * 1e6);
        result.completed = (retired >= instruction_count);
        // Clean up if the simulation exited early
        while (!q.empty()) {
            delete q.front();
            q.pop_front(); 
        }

        return result;
    }

private:
    pipelineConfiguration config;

    // dep_ready[producer_pc] = earliest cycle a dependent instruction may enter EX.
    // Integer / Floating-point / Branch producers: set to cycle_EX_done + 1 when they enter EX
    // Load / Store producers:set to LLONG_MAX when entering EX then updated to cycle_MEM_done + 1 when entering MEM
    unordered_map<unsigned long long, long long> dep_ready;
};

// Main Function
// Parses command line arguments and runs one simulation
int main(int argc,char* argv[]) {
    if (argc!=5) {
        cerr << "Wrong usage!" << endl;
        return 1;
    }

    string trace_path = argv[1];
    long long start_instruction = stoll(argv[2]); // 1-based index of the first instruction to simulate
    long long instruction_count = stoll(argv[3]); // number of instructions to simulate
    int D= stoi(argv[4]); // pipeline depth configuration: 1, 2, 3, or 4

    if (start_instruction < 1) {
        cerr << "Invalid start_instruction: must be >= 1" << endl;
        return 1;
    }
    if (instruction_count < 0) {
        cerr << "Invalid instruction_count: must be >= 0" << endl;
        return 1;
    }

    pipelineConfiguration config = makeConfig(D);
    TraceReader reader(trace_path,start_instruction);
    if (!reader.isOpen()) {
        return 1;
    }
    Simulator sim(config);
    SimulationResult result = sim.run(reader, instruction_count);
    if (!result.completed) {
        cerr << "Error: trace ended before simulating requested instruction count." << endl;
        return 1;
    }
    printStats(result);

    return 0;
}
