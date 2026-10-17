#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace std;

int processNum; 
const int commands = 100;
const int cores = 4;
// Holds info for processes
struct Process {
    string name;
    string status;
    int core;
    int completedPrints;
};

vector<Process> processes;
queue<int> waitingQueue; // Holds process numbers in FCFS order.
mutex dataMutex;
mutex timeMutex;
condition_variable queueChanged;
bool schedulerFinished = false;

// For Time and date formatting
string getTimestamp() {
    lock_guard<mutex> lock(timeMutex);
    time_t now = time(nullptr);
    tm *timeInfo = localtime(&now);
    stringstream output;
    output << put_time(timeInfo, "%m/%d/%Y %I:%M:%S%p");
    return output.str();
}

// The scheduler puts processes into the queue in FCFS order.
void schedulerThread() {
    for (int i = 0; i < processNum; i++) {
        {
            lock_guard<mutex> lock(dataMutex);
            waitingQueue.push(i);
        }
        queueChanged.notify_one();
    }
    {
        lock_guard<mutex> lock(dataMutex);
        schedulerFinished = true;
    }
    queueChanged.notify_all();
}

// Each worker represents one CPU core.
void cpuWorker(int coreNumber) {
    while (true) {
        int processNumber;

        {
            unique_lock<mutex> lock(dataMutex);
            queueChanged.wait(lock, [] {
                return !waitingQueue.empty() || schedulerFinished;
            });

            if (waitingQueue.empty() && schedulerFinished) {
                return;
            }

            // Take the oldest waiting process first.
            processNumber = waitingQueue.front();
            waitingQueue.pop();
            processes[processNumber].status = "Running";
            processes[processNumber].core = coreNumber;
        }

        // for text file generation
        string fileName = processes[processNumber].name + ".txt";
        ofstream file(fileName);

        for (int i = 1; i <= commands; i++) {
            string timestamp = getTimestamp();
            file << " | Timestamp: " << timestamp
                 << " | Core: " << coreNumber
                 << " | Hello World! #" << i << endl;

            {
                lock_guard<mutex> lock(dataMutex);
                processes[processNumber].completedPrints = i;
            }

            // delay for running screen ls mid-run
            this_thread::sleep_for(chrono::milliseconds(50));
        }

        file.close();

        // Mark this process as finished.
        {
            lock_guard<mutex> lock(dataMutex);
            processes[processNumber].status = "Finished";
            processes[processNumber].core = -1;
        }
    }
}

// Displays the current process list, similar to the reference layout.
void showProcessList() {
    lock_guard<mutex> lock(dataMutex);

    cout << "\n------------------------------------------------------------\n";
    cout << "Running processes:\n";

    bool hasRunning = false;
    for (int i = 0; i < processNum; i++) {
        if (processes[i].status == "Running") {
            hasRunning = true;
            cout << processes[i].name << "  Core: " << processes[i].core
                 << "  " << processes[i].completedPrints << " / "
                 << commands << "\n";
        }
    }
    if (!hasRunning) cout << "None\n";

    cout << "\nFinished processes:\n";
    bool hasFinished = false;
    for (int i = 0; i < processNum; i++) {
        if (processes[i].status == "Finished") {
            hasFinished = true;
            cout << processes[i].name << "  Finished  "
                 << commands << " / " << commands << "\n";
        }
    }
    if (!hasFinished) cout << "None\n";
    cout << "------------------------------------------------------------\n";
}

int main() {
    // Ask the user how many processes to create.
    cout << "Enter Processes ";
    cin >> processNum;

    // Check that the user entered a valid positive number.
    while (cin.fail() || processNum < 1) {
        cin.clear();
        cin.ignore(10000, '\n');
        cout << "Please enter a number greater than 0: ";
        cin >> processNum;
    }
    cin.ignore(10000, '\n'); // Remove the leftover enter key 

    // create process then store in struct
    for (int i = 0; i < processNum; i++) {
        Process process;
        stringstream name;
        name << "process" << setw(2) << setfill('0') << (i + 1);
        process.name = name.str();
        process.status = "Waiting";
        process.core = -1;
        process.completedPrints = 0;
        processes.push_back(process);
    }

    cout << "Group 5 - FCFS Emulator\n";
    cout << processNum << " processes created. Each process has 100 print commands.\n";
    cout << "Commands: screen -ls, exit\n";

    // Start the scheduler thread and one worker thread per CPU core.
    thread scheduler(schedulerThread);
    vector<thread> workers;
    for (int core = 0; core < cores; core++) {
        workers.push_back(thread(cpuWorker, core));
    }

    // Read commands from the user.
    string command;
    while (true) {
        cout << "> ";
        if (!getline(cin, command)) command = "exit";
        if (command == "screen -ls") {
            showProcessList();
        } else if (command == "exit") {
            break;
        }
    }

    scheduler.join();
    for (int i = 0; i < workers.size(); i++) {
        workers[i].join();
    }

    cout << "Text Files generated\n";
    return 0;
}