#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <conio.h>
#include <windows.h>

using namespace std;

const int MARQUEE_HEIGHT = 10; // number of rows the text bounces around in
const int MARQUEE_WIDTH = 40;  // number of columns the text bounces around in

mutex consoleMutex; // only one thread may write to the console at a time

// Thread-safe printing
void print(string text){
    consoleMutex.lock();
    cout << text << flush;
    consoleMutex.unlock();
}

// Header Printing Function
void printHeader(){
    system("color 0B");

    cout << " ===============================================================" << endl;
    cout << "   ____ ____   ___  ____  _____ ______   __" << endl;
    cout << "  / ___/ ___| / _ \\|  _ \\| ____/ ___\\ \\ / /" << endl;
    cout << " | |   \\___ \\| | | | |_) |  _| \\___ \\\\ V /" << endl;
    cout << " | |___ ___) | |_| |  __/| |___ ___) || |" << endl;
    cout << "  \\____|____/ \\___/|_|   |_____|____/ |_|" << endl;
    cout << endl;
    cout << " ===============================================================" << endl;
    cout << endl;
    cout << "Hello, welcome to CSOPESY commandline!" << endl;
    cout << endl;
    cout << "Group developer:" << endl;
    cout << "  BALILA, DALE VERNARD" << endl;
    cout << "  CALDERON, JOHN GABRIEL" << endl;
    cout << "  ENCARNACION, ALESSANDRO GABRIEL" << endl;
    cout << endl;
    cout << "Version date: September 28, 2026" << endl;
    cout << endl;
    cout << "Type 'help' to see the available commands." << endl;
    cout << endl;
}

// Help Printing Function
void printHelp(){
    print("\nCommands List:\n"
          "  help          - Displays available commands\n"
          "  start_marquee - Starts the marquee animation\n"
          "  stop_marquee  - Stops the marquee animation\n"
          "  set_text      - Changes the marquee text\n"
          "  set_speed     - Changes the marquee speed\n"
          "  clear         - Clears the screen\n"
          "  exit          - Exits the program\n\n");
}

// Reads a line by polling the keyboard, echoing each key through print()
// so typed characters never get drawn on the marquee line
string readLine(){
    string line;

    while (true){
        if (_kbhit()){
            int ch = _getch();

            if (ch == 0 || ch == 224){ // ignore arrow/function keys
                _getch();
            }
            else if (ch == '\r'){ // Enter
                print("\n");
                return line;
            }
            else if (ch == '\b'){ // Backspace
                if (!line.empty()){
                    line.pop_back();
                    print("\b \b");
                }
            }
            else if (ch >= 32 && ch < 127){ // normal character
                line += (char)ch;
                print(string(1, (char)ch));
            }
        }
        else{
            Sleep(10); // polling rate: check the keyboard every 10 ms
        }
    }
}

int main(){
    // Flag to control the main loop
    int flag = 1;
    string command;

    string marqueeText = "Hello World";
    atomic<int> marqueeSpeed(100); // atomic: read by the marquee thread
    mutex textMutex;               // protects marqueeText between threads

    atomic<bool> running(false);
    thread marqueeThread;

    printHeader();

    // Reserve a line for the marquee
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleScreenBufferInfo(console, &consoleInfo);
    COORD marqueePosition = consoleInfo.dwCursorPosition; // top-left of the area

    for (int row = 0; row < MARQUEE_HEIGHT; row++){
        cout << endl;
    }

    // Main command loop
    while (flag == 1){
        print("\nCommand> ");
        command = readLine();

        if (command == "help"){
            printHelp();
        }

        else if (command == "start_marquee"){
            if (!running){
                running = true;
 
                marqueeThread = thread([&](){
                    int x = 0, y = 0;   // text position inside the area
                    int dx = 1, dy = 1; // direction: right and down
 
                    while (running){
                        // Re-read the text every frame so set_text updates it live
                        textMutex.lock();
                        string text = marqueeText;
                        textMutex.unlock();
 
                        consoleMutex.lock();
 
                        CONSOLE_SCREEN_BUFFER_INFO info;
                        GetConsoleScreenBufferInfo(console, &info);
 
                        // Keep the text inside the console width
                        int width = MARQUEE_WIDTH;
                        if (width > info.dwSize.X - 1){
                            width = info.dwSize.X - 1;
                        }
                        if ((int)text.length() > width){
                            text = text.substr(0, width);
                        }
 
                        int maxX = width - text.length(); // furthest column
                        int maxY = MARQUEE_HEIGHT - 1;    // furthest row
 
                        // Stay in bounds if the text got longer
                        if (x > maxX) x = maxX;
                        if (y > maxY) y = maxY;
 
                        // Redraw the whole marquee box (every row is spaces except the one with the text)
                        for (int row = 0; row < MARQUEE_HEIGHT; row++){
                            string line(width, ' ');
                            if (row == y){
                                line.replace(x, text.length(), text);
                            }
 
                            COORD rowPosition = {0, (SHORT)(marqueePosition.Y + row)};
                            DWORD written;
                            WriteConsoleOutputCharacterA(console, line.c_str(), line.length(), rowPosition, &written);
                        }
 
                        consoleMutex.unlock();
 
                        // Move diagonally and bounce off the edges
                        x += dx;
                        y += dy;
                        if (x <= 0)    dx = 1;
                        if (x >= maxX) dx = -1;
                        if (y <= 0)    dy = 1;
                        if (y >= maxY) dy = -1;
 
                        // Wait in short steps so stop_marquee responds right away
                        auto wakeTime = chrono::steady_clock::now()
                                      + chrono::milliseconds(marqueeSpeed.load());
                        while (running && chrono::steady_clock::now() < wakeTime){
                            this_thread::sleep_for(chrono::milliseconds(5));
                        }
                    }
                });
 
                print("Marquee started.\n");
            }
            else{
                print("Marquee is already running.\n");
            }
        }

        else if (command == "stop_marquee"){
            if (running){
                running = false;
                marqueeThread.join();

                // Clear the marquee line, then return the cursor where it was
                consoleMutex.lock();
                CONSOLE_SCREEN_BUFFER_INFO info;
                GetConsoleScreenBufferInfo(console, &info);
                
                for (int row = 0; row < MARQUEE_HEIGHT; row++){
                    string blank(info.dwSize.X - 1, ' ');
					COORD rowPosition = {0, (SHORT)(marqueePosition.Y + row)};
					DWORD written;
                    WriteConsoleOutputCharacterA(console, blank.c_str(), blank.length(), rowPosition, &written);
                }

                consoleMutex.unlock();

                print("Marquee stopped.\n");
            }
            else{
                print("Marquee is not running.\n");
            }
        }

        else if (command == "set_text"){
            print("Enter the new marquee text: ");
            string newText = readLine();

            textMutex.lock();
            marqueeText = newText;
            textMutex.unlock();

            print("Marquee text set to: " + newText + "\n");
        }

        else if (command == "set_speed"){
            print("Enter the marquee speed in milliseconds: ");
            int speed = atoi(readLine().c_str()); // non-numbers become 0

            if (speed < 1){
                speed = 1;
            }
            marqueeSpeed = speed;

            print("Marquee speed set to: " + to_string(speed) + " ms\n");
        }

        else if (command == "clear"){
            consoleMutex.lock();
            system("cls");
            printHeader();

            // The screen was wiped, so find the new marquee line
            GetConsoleScreenBufferInfo(console, &consoleInfo);
            marqueePosition = consoleInfo.dwCursorPosition;
            for (int row = 0; row < MARQUEE_HEIGHT; row++){
                cout << endl;
            }
            consoleMutex.unlock();
        }

        else if (command == "exit"){
            running = false;

            if (marqueeThread.joinable()){
                marqueeThread.join();
            }

            flag = 0;
        }

        else{
            print("Invalid Command\n");
        }
    }

    return 0;
}
