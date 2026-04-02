#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <map>
using namespace std;

// Data structure
struct Instruction {
    unsigned long long pc;
    int type; // where 1:Int, 2:FP, 3:Br, 4:Ld, 5:St
    vector<unsigned long long> dep_pcs;
    int stage; // where 0:IF, 1:ID, 2:EX,3:MEM, 4:WB, 5:Retired
    long long ready_cycle; 
};

// 
class TradeReader {

}

// 
class Simulatior {
    // The Pipeline
    vector<Instruction*> stage_IF, stage_ID, stage_EX, stage_MEM, stage_WB;
}

int main(int argc, char* argv[]) {
    // ./sim trace.txt 1 (for D=1)
    string tracePath = argv[1];
    int configD = stoi(argv[2]);

    TraceReader reader(tracePath);
    Simulator sim;
    cout << "Starting Simulation for D=" << configD << "..." << endl;
    sim.run(reader, 1000000);

    // Final Stats
    cout << "Total Cycles: " << sim.cycles << endl;

    return 0;