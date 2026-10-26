// CSOPESY MO1 - Process Scheduler and CLI

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <cstdint>

using namespace std;

/* CONFIGURATION */
int numCpu = 4;                       // [1, 128]
string schedulerType = "rr";          // fcfs or rr
unsigned long long quantumCycles = 5; // RR time slice in CPU ticks
unsigned long long batchFreq = 1;     // new process every X ticks (scheduler-start)
unsigned long long minIns = 1000;     // instructions per process (min)
unsigned long long maxIns = 2000;     // instructions per process (max)
unsigned long long delayPerExec = 0;  // busy-wait ticks after each instruction
int tickMs = 50;                      // real milliseconds per CPU tick (optional key: tick-ms)

/* PROCESS INSTRUCTIONS */
enum InstructionType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

// an operand is either a variable name or a literal value
struct Operand {
    bool isVar = false;
    string name;
    int value = 0;
};

struct Instruction {
    InstructionType type;     // DECLARE/ADD/SUBTRACT/PRINT/SLEEP/FOR
    string var;               // DECLARE/ADD/SUBTRACT: target variable. PRINT: variable to print.
    Operand a, b;             // DECLARE: uses a.value only. ADD/SUBTRACT: var = a +/- b.
    int sleepTicks = 0;       // SLEEP(X)
    vector<Instruction> body; // FOR: instructions to repeat
    int repeats = 0;          // FOR: number of repeats
};

/* PROCESS REPRESENTATION */
enum ProcessState { READY, RUNNING, SLEEPING, FINISHED };

// one level of the call stack used to run nested FOR loops
struct Frame {
    const vector<Instruction>* list; // current instructions being executed
    size_t index;                    // next instruction to run
    int repeatsLeft;                 // loop repetitions left
};

struct Process {
    string name;
    int id;
    string createdAt, finishedAt;

    unsigned long long totalLines = 0;    // lines of code
    unsigned long long executedLines = 0; // current instruction line

    vector<Instruction> instructions;
    bool generated = false; // instructions are created when first scheduled
    vector<Frame> stack;

    map<string, uint16_t> variables; // kept until the process finishes
    vector<string> logs;             // output of PRINT instructions

    ProcessState state = READY;
    int core = -1;
    unsigned long long delayLeft = 0;   // busy-wait ticks left
    unsigned long long quantumUsed = 0; // RR ticks used in this time slice
    unsigned long long wakeTick = 0;    // CPU tick when a SLEEPING process goes back to the ready queue
};

/* SHARED OS STATE */
mutex osMutex;
vector<Process*> allProcesses;
map<string, Process*> processByName;
deque<Process*> readyQueue;
vector<Process*> sleepingProcesses;
vector<Process*> cores; // cores[i] = process on core i

unsigned long long cpuTicks = 0;
unsigned long long ticksSinceBatch = 0;
int nextProcessId = 1;
int generatedCount = 0;

atomic<bool> osRunning(false);
atomic<bool> generating(false); // scheduler-start / scheduler-stop
bool initialized = false;
thread cpuThread;

mt19937 rng(random_device{}());

/* HELPERS */
unsigned long long randomInt(unsigned long long low, unsigned long long high){
    if (low >= high) return low;
    uniform_int_distribution<unsigned long long> dist(low, high);
    return dist(rng);
}

string timestamp(){
    time_t now = time(nullptr);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", localtime(&now));
    return buffer;
}

uint16_t clampUint16(long long value){
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return (uint16_t)value;
}

void clearScreen(){
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

vector<string> splitWords(const string& line){
    vector<string> words;
    istringstream in(line);
    string word;
    while (in >> word){
        words.push_back(word);
    }
    return words;
}

/* RANDOM INSTRUCTION GENERATION */
const string VAR_NAMES[] = {"x", "y", "z", "a", "b"};

string randomVar(){
    return VAR_NAMES[randomInt(0, 4)];
}

Operand randomOperand(){
    Operand op;
    if (randomInt(0, 1) == 0){
        op.isVar = true;
        op.name = randomVar();
    }
    else{
        op.value = (int)randomInt(0, 100);
    }
    return op;
}

Instruction randomSimpleInstruction(){
    Instruction ins;
    ins.type = (InstructionType)randomInt(0, 4); // 0=PRINT/1=DECLARE/2=ADD/3=SUBTRACT/4=SLEEP
    ins.var = randomVar();

    if (ins.type == PRINT){
        ins.var = ""; // default "<name> is running..." message
    }
    else if (ins.type == DECLARE){
        ins.a.value = (int)randomInt(0, 100);
    }
    else if (ins.type == ADD || ins.type == SUBTRACT){
        ins.a = randomOperand();
        ins.b = randomOperand();
    }
    else if (ins.type == SLEEP){
        ins.sleepTicks = (int)randomInt(1, 10);
    }
    return ins;
}

// Builds a list that executes exactly 'count' instructions.
// FOR loops can be nested up to 3 levels.
vector<Instruction> generateInstructions(unsigned long long count, int depth){
    vector<Instruction> list;

    while (count > 0){
        if (depth < 3 && count >= 2 && randomInt(1, 10) == 1){
            Instruction loop;
            loop.type = FOR;
            loop.repeats = (int)randomInt(2, min(3ULL, count));
            unsigned long long bodySize = randomInt(1, min(10ULL, count / loop.repeats));
            loop.body = generateInstructions(bodySize, depth + 1);
            count -= bodySize * loop.repeats;
            list.push_back(loop);
        }
        else{
            list.push_back(randomSimpleInstruction());
            count--;
        }
    }
    return list;
}

/* INSTRUCTION EXECUTION */
uint16_t valueOf(Process* p, const Operand& op){
    if (op.isVar){
        return p->variables[op.name]; // undeclared variables start at 0
    }
    return clampUint16(op.value);
}

void executeInstruction(Process* p, const Instruction& ins, int coreId){
    if (ins.type == PRINT){
        string msg;
        if (ins.var.empty()){
            msg = p->name + " is running...";
        }
        else{
            msg = "Value from: " + to_string(p->variables[ins.var]);
        }
        p->logs.push_back("(" + timestamp() + ") Core:" + to_string(coreId) + " \"" + msg + "\"");
    }
    else if (ins.type == DECLARE){
        p->variables[ins.var] = clampUint16(ins.a.value);
    }
    else if (ins.type == ADD){
        p->variables[ins.var] = clampUint16((long long)valueOf(p, ins.a) + valueOf(p, ins.b));
    }
    else if (ins.type == SUBTRACT){
        p->variables[ins.var] = clampUint16((long long)valueOf(p, ins.a) - valueOf(p, ins.b));
    }
    else if (ins.type == SLEEP){
        p->wakeTick = cpuTicks + ins.sleepTicks;
    }
}

// Runs the next instruction of a process. Returns true if it was a SLEEP.
bool runNextInstruction(Process* p, int coreId){
    while (!p->stack.empty()){
        Frame& top = p->stack.back();

        // End of this level: repeat the loop, or go back up a level
        if (top.index >= top.list->size()){
            top.repeatsLeft--;
            if (top.repeatsLeft > 0){
                top.index = 0;
            }
            else{
                p->stack.pop_back();
            }
            continue;
        }

        const Instruction& ins = (*top.list)[top.index];
        top.index++;

        // Entering a FOR loop: push its body onto the stack
        if (ins.type == FOR){
            p->stack.push_back({&ins.body, 0, ins.repeats});
            continue;
        }

        executeInstruction(p, ins, coreId);
        p->executedLines++;
        return ins.type == SLEEP && ins.sleepTicks > 0;
    }
    return false;
}

/* PROCESS CREATION */
Process* createProcess(const string& name){
    Process* p = new Process;
    p->name = name;
    p->id = nextProcessId++;
    p->createdAt = timestamp();
    p->totalLines = randomInt(minIns, maxIns);

    allProcesses.push_back(p);
    processByName[name] = p;
    readyQueue.push_back(p);
    return p;
}

string nextGeneratedName(){
    string name;
    do{
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "p%02d", ++generatedCount);
        name = buffer;
    } while (processByName.count(name) > 0);
    return name;
}

/* SCHEDULER: ONE CPU TICK */
void removeFromCore(int c){
    cores[c]->core = -1;
    cores[c] = nullptr;
}

void cpuTick(){
    // 1. Wake up sleeping processes
    for (size_t i = 0; i < sleepingProcesses.size(); ){
        Process* p = sleepingProcesses[i];
        if (cpuTicks >= p->wakeTick){
            p->state = READY;
            readyQueue.push_back(p);
            sleepingProcesses.erase(sleepingProcesses.begin() + i);
        }
        else{
            i++;
        }
    }

    // 2. Every busy core executes one step of its process
    for (int c = 0; c < numCpu; c++){
        Process* p = cores[c];
        if (p == nullptr) continue;

        bool slept = false;
        if (p->delayLeft > 0){
            p->delayLeft--; // busy-waiting: stays on the core
        }
        else{
            slept = runNextInstruction(p, c);
            p->delayLeft = delayPerExec;
        }
        p->quantumUsed++;

        if (p->executedLines >= p->totalLines){
            // finished (free its memory and the core)
            p->state = FINISHED;
            p->finishedAt = timestamp();
            p->instructions.clear();
            p->instructions.shrink_to_fit();
            p->stack.clear();
            p->variables.clear();
            removeFromCore(c);
        }
        else if (slept){
            // SLEEP relinquishes the CPU
            p->state = SLEEPING;
            p->delayLeft = 0;
            sleepingProcesses.push_back(p);
            removeFromCore(c);
        }
        else if (schedulerType == "rr" && p->quantumUsed >= quantumCycles){
            // Round Robin: time slice used up, back of the queue
            p->state = READY;
            readyQueue.push_back(p);
            removeFromCore(c);
        }
    }

    // 3. scheduler-start: new process every batchFreq ticks
    if (generating){
        ticksSinceBatch++;
        if (ticksSinceBatch >= batchFreq){
            createProcess(nextGeneratedName());
            ticksSinceBatch = 0;
        }
    }

    // 4. Give idle cores the next process in the ready queue (done last so no core sits idle between ticks)
    for (int c = 0; c < numCpu; c++){
        if (cores[c] == nullptr && !readyQueue.empty()){
            Process* p = readyQueue.front();
            readyQueue.pop_front();

            if (!p->generated){
                p->instructions = generateInstructions(p->totalLines, 0);
                p->stack.push_back({&p->instructions, 0, 1});
                p->generated = true;
            }
            p->state = RUNNING;
            p->core = c;
            p->quantumUsed = 0;
            cores[c] = p;
        }
    }

    cpuTicks++;
}

void cpuLoop(){
    while (osRunning){
        {
            lock_guard<mutex> lock(osMutex);
            cpuTick();
        }
        this_thread::sleep_for(chrono::milliseconds(tickMs));
    }
}

/* config.txt */
bool loadConfig(const string& path){
    ifstream file(path);
    if (!file){
        cout << "Error: " << path << " not found." << endl;
        return false;
    }

    string key, value;
    while (file >> key >> value){
        value.erase(remove(value.begin(), value.end(), '"'), value.end()); // "rr" -> rr

        unsigned long long n = 0;
        try { n = stoull(value); } catch (...) {}

        if (key == "num-cpu")                                         
            numCpu = (int)max(1ULL, min(128ULL, n));
        else if (key == "scheduler")                                  
            schedulerType = value;
        else if (key == "quantum-cycles")                             
            quantumCycles = max(1ULL, n);
        else if (key == "batch-process-freq")                         
            batchFreq = max(1ULL, n);
        else if (key == "min-ins")                                    
            minIns = max(1ULL, n);
        else if (key == "max-ins")                                    
            maxIns = max(1ULL, n);
        else if (key == "delay-per-exec" || key == "delays-per-exec") 
            delayPerExec = n;
        else if (key == "tick-ms")                                    
            tickMs = (int)n;
    }

    if (schedulerType != "fcfs" && schedulerType != "rr"){
        cout << "Error: scheduler must be \"fcfs\" or \"rr\"." << endl;
        return false;
    }
    if (minIns > maxIns){
        swap(minIns, maxIns);
    }
    return true;
}

/* CONSOLE UI */
void printHeader(){
    cout << "   ____ ____   ___  ____  _____ ______   __" << endl;
    cout << "  / ___/ ___| / _ \\|  _ \\| ____/ ___\\ \\ / /" << endl;
    cout << " | |   \\___ \\| | | | |_) |  _| \\___ \\\\ V /" << endl;
    cout << " | |___ ___) | |_| |  __/| |___ ___) || |" << endl;
    cout << "  \\____|____/ \\___/|_|   |_____|____/ |_|" << endl;
    cout << "--------------------------------------------" << endl;
    cout << "Welcome to CSOPESY Emulator!" << endl;
    cout << endl;
    cout << "Developers:" << endl;
    cout << "  Balila, Dale" << endl;
    cout << "  Calderon, Gabe" << endl;
    cout << "  Encarnacion, Andrei" << endl;
    cout << endl;
    cout << "Last updated: 10-26-2026" << endl;
}

// Builds the "screen -ls" / "report-util" text
string buildReport(){
    ostringstream out;

    int used = 0;
    for (Process* p : cores){
        if (p != nullptr) used++;
    }

    out << "CPU utilization: " << used * 100 / numCpu << "%" << endl;
    out << "Cores used: " << used << endl;
    out << "Cores available: " << numCpu - used << endl;
    out << "Processes waiting: " << readyQueue.size() + sleepingProcesses.size() << endl;
    out << endl;
    out << "--------------------------------------------" << endl;

    out << "Running processes:" << endl;
    for (int c = 0; c < numCpu; c++){
        Process* p = cores[c];
        if (p == nullptr) continue;
        out << left << setw(12) << p->name << "(" << p->createdAt << ")   "
            << "Core: " << setw(4) << c << p->executedLines << " / " << p->totalLines << endl;
    }

    out << endl;
    out << "Finished processes:" << endl;
    for (Process* p : allProcesses){
        if (p->state != FINISHED) continue;
        out << left << setw(12) << p->name << "(" << p->finishedAt << ")   "
            << "Finished  " << p->totalLines << " / " << p->totalLines << endl;
    }
    out << "--------------------------------------------" << endl;

    return out.str();
}

// "process-smi" output
void showProcessInfo(Process* p){
    cout << "Process name: " << p->name << endl;
    cout << "ID: " << p->id << endl;
    cout << "Logs:" << endl;
    for (const string& log : p->logs){
        cout << log << endl;
    }
    cout << endl;

    if (p->state == FINISHED){
        cout << "Finished!" << endl;
    }
    else{
        cout << "Current instruction line: " << p->executedLines << endl;
        cout << "Lines of code: " << p->totalLines << endl;
    }
}

// The process screen ("screen -s" / "screen -r")
void processScreen(Process* p){
    clearScreen();
    {
        lock_guard<mutex> lock(osMutex);
        showProcessInfo(p);
    }

    while (true){
        cout << endl << "root:\\> ";
        string line;
        if (!getline(cin, line)) return;
        vector<string> words = splitWords(line);
        if (words.empty()) continue;

        if (words[0] == "process-smi"){
            cout << endl;
            lock_guard<mutex> lock(osMutex);
            showProcessInfo(p);
        }
        else if (words[0] == "exit"){
            clearScreen();
            printHeader();
            return;
        }
        else{
            cout << "Unknown command. Use 'process-smi' or 'exit'." << endl;
        }
    }
}

/* MAIN MENU */
int main(){
    printHeader();

    while (true){
        cout << endl << "root:\\> ";
        string line;
        if (!getline(cin, line)) break;
        vector<string> words = splitWords(line);
        if (words.empty()) continue;

        string command = words[0];

        if (command == "exit"){
            break;
        }

        // nothing else works until "initialize" is run
        if (!initialized && command != "initialize"){
            cout << "Please run 'initialize' first." << endl;
            continue;
        }

        if (command == "initialize"){
            if (initialized){
                cout << "Already initialized." << endl;
            }
            else if (loadConfig("config.txt")){
                cores.assign(numCpu, nullptr);
                initialized = true;
                osRunning = true;
                cpuThread = thread(cpuLoop); // CPU ticks start now

                cout << "Initialized: " << numCpu << " CPU(s), scheduler " << schedulerType
                     << ", quantum " << quantumCycles << ", batch freq " << batchFreq
                     << ", instructions " << minIns << "-" << maxIns
                     << ", delay " << delayPerExec << "." << endl;
            }
        }

        else if (command == "screen"){
            if (words.size() >= 2 && words[1] == "-ls"){
                lock_guard<mutex> lock(osMutex);
                cout << buildReport();
            }
            else if (words.size() >= 3 && words[1] == "-s"){
                string name = words[2];
                Process* p = nullptr;
                {
                    lock_guard<mutex> lock(osMutex);
                    if (processByName.count(name) > 0){
                        cout << "Process " << name << " already exists." << endl;
                    }
                    else{
                        p = createProcess(name);
                    }
                }
                if (p != nullptr) processScreen(p);
            }
            else if (words.size() >= 3 && words[1] == "-r"){
                string name = words[2];
                Process* p = nullptr;
                {
                    lock_guard<mutex> lock(osMutex);
                    if (processByName.count(name) > 0 && processByName[name]->state != FINISHED){
                        p = processByName[name];
                    }
                }
                if (p != nullptr) processScreen(p);
                else cout << "Process " << name << " not found." << endl;
            }
            else{
                cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << endl;
            }
        }

        else if (command == "scheduler-start"){
            if (generating){
                cout << "Scheduler is already generating processes." << endl;
            }
            else{
                {
                    lock_guard<mutex> lock(osMutex);
                    ticksSinceBatch = 0;
                }
                generating = true;
                cout << "Scheduler started: a new process every " << batchFreq << " CPU tick(s)." << endl;
            }
        }

        else if (command == "scheduler-stop"){
            if (generating){
                generating = false;
                cout << "Scheduler stopped generating processes." << endl;
            }
            else{
                cout << "Scheduler is not running." << endl;
            }
        }

        else if (command == "report-util"){
            string report;
            {
                lock_guard<mutex> lock(osMutex);
                report = buildReport();
            }
            ofstream file("csopesy-log.txt");
            file << report;
            file.close();
            cout << "Report generated at " << filesystem::absolute("csopesy-log.txt").string() << "!" << endl;
        }

        else{
            cout << "Command not recognized: " << command << endl;
        }
    }

    // shut down the CPU thread before exiting
    generating = false;
    osRunning = false;
    if (cpuThread.joinable()){
        cpuThread.join();
    }
    for (Process* p : allProcesses){
        delete p;
    }
    return 0;
}
