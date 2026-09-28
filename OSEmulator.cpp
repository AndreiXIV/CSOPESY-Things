
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <windows.h>

using namespace std;

//Header Printing Function
void printHeader(){
    system("color 0B");

    cout << " ===============================================================" << endl;
    cout << "   _____  ____   ___   ____  _____  ____  _   __" << endl;
    cout << "  / ____|/ ___| / _ \\ |  _ \\| ____|/ ___|\\ \\ / /" << endl;
    cout << " | |     \\___| | | | |  |_) |  _|  \\___ \\ \\ V / " << endl;
    cout << " | |____  ___) | |_| ||  __/| |___  ___) | | |  " << endl;
    cout << "  \\_____||____/ \\___/ |_|   |_____||____/  |_|  " << endl;
    cout << endl;
    cout << " ===============================================================" << endl;
    cout << endl;
    cout << "Hello, welcome to CSOPESY commandline!" << endl;
    cout << endl;
}

// Help Printing Function
void printHelp(){
    cout << endl;
    cout << "Commands List:" << endl;
    cout << "  help           - Displays available commands" << endl;
    cout << "  start_marquee  - Starts the marquee animation" << endl;
    cout << "  stop_marquee   - Stops the marquee animation" << endl;
    cout << "  set_text       - Changes the marquee text" << endl;
    cout << "  set_speed      - Changes the marquee speed" << endl;
    cout << "  exit           - Exits the program" << endl;
    cout << endl;
}

int main(){
    // Flag to control the main loop
    int flag = 1;
    string command;

    string marqueeText = "Hello World";
    int marqueeSpeed = 100;

    atomic<bool> running(false);
    thread marqueeThread;

    printHeader();

    // Reserve a line for the marquee
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleScreenBufferInfo(console, &consoleInfo);
    COORD marqueePosition = consoleInfo.dwCursorPosition;

    cout << endl;

    // Main command loop
    while (flag == 1){
        cout << "\nEnter a command: ";
        cin >> command;

        // Convert command to lowercase for case-insensitive comparison
        if (command == "help"){
            printHelp();
        }

        else if (command == "start_marquee"){
            if (!running){
                running = true;

                marqueeThread = thread([&](){
                    string displayText = marqueeText + "     ";
                    int position = 0;

                    while (running){
                        CONSOLE_SCREEN_BUFFER_INFO info;
                        GetConsoleScreenBufferInfo(console, &info);

                        // Move to the marquee line
                        SetConsoleCursorPosition(console, marqueePosition);

                        // Display the moving text
                        string output = displayText.substr(position)
                                      + displayText.substr(0, position);

                        cout << output << "          " << flush;

                        // Restore the previous cursor position
                        SetConsoleCursorPosition(
                            console, info.dwCursorPosition
                        );

                        position++;

                        if (position >= displayText.length()){
                            position = 0;
                        }

                        this_thread::sleep_for(
                            chrono::milliseconds(marqueeSpeed)
                        );
                    }
                });

                cout << "Marquee started." << endl;
            }
            else{
                cout << "Marquee is already running." << endl;
            }
        }

        else if (command == "stop_marquee"){
            if (running){
                running = false;
                marqueeThread.join();

                SetConsoleCursorPosition(console, marqueePosition);
                cout << "                         " << flush;

                cout << "\nMarquee stopped." << endl;
            }
            else{
                cout << "Marquee is not running." << endl;
            }
        }

        else if (command == "set_speed"){
            cout << "Enter the marquee speed in milliseconds: ";
            cin >> marqueeSpeed;

            if (marqueeSpeed < 1){
                marqueeSpeed = 1;
            }

            cout << "Marquee speed set to: "
                 << marqueeSpeed << " ms" << endl;
        }

        else if (command == "set_text"){
            cout << "Enter the new marquee text: ";
            cin.ignore();
            getline(cin, marqueeText);

            cout << "Marquee text set to: "
                 << marqueeText << endl;
        }


        else if (command == "exit"){
            running = false;

            if (marqueeThread.joinable()){
                marqueeThread.join();
            }

            flag = 0;
        }

        else{
            cout << "Invalid Command" << endl;
        }
    }

    return 0;
}