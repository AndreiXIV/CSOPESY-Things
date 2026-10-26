CSOPESY MO1 - Process Scheduler and CLI

Group 5:
- Balila, Dale
- Calderon, Gabe
- Encarnacion, Andrei

Entry file: main.cpp (contains the main() function)

HOW TO BUILD AND RUN (config.txt must be in the same folder as main.cpp):
  Windows:
    1. Open cmd in this folder.
    2. Build Command: g++ -std=c++17 -O2 main.cpp -o CSOPESY-MO1.exe
    3. Run Command: CSOPESY-MO1.exe
  macOS:
    1. If not yet installed, run this in Terminal: xcode-select --install
    2. Open Terminal in this folder.
    3. Build Command: g++ -std=c++17 -O2 main.cpp -o CSOPESY-MO1
    4. Run Command: ./CSOPESY-MO1

COMMANDS (main menu):
- initialize       - reads config.txt (required before anything, except exit)
- screen -s <name> - create a process and open its screen
- screen -r <name> - reopen a running process's screen
- screen -ls       - CPU utilization + running/finished processes
- scheduler-start  - generate a dummy process every batch-process-freq ticks
- scheduler-stop   - stop generating processes
- report-util      - save the screen -ls report to csopesy-log.txt
- exit             - quit

COMMANDS (inside a process screen):
- process-smi - process info / PRINT logs / progress
- exit        - back to the main menu

CONFIG.TXT:
- num-cpu
- scheduler (fcfs/rr)
- quantum-cycles
- batch-process-freq
- min-ins
- max-ins
- delay-per-exec
- tick-ms <n> (optional: real milliseconds per CPU tick [default=50])