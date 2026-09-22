#include <iostream>
#include <string>

using namespace std;

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

int main(){
    int flag = 1;
    string command;

    printHeader();

    while (flag == 1){
        cout << "\nEnter a command: ";
        cin >> command;

        if (command == "initialize"){
            cout << "initialize command recognized. Doing something." << endl;
        }

        else if (command == "screen"){
            cout << "screen command recognized. Doing something." << endl;
        }

        else if (command == "scheduler-start"){
            cout << "scheduler-start command recognized. Doing something." << endl;
        }

        else if (command == "scheduler-stop"){
            cout << "scheduler-stop command recognized. Doing something." << endl;
        }

        else if (command == "report-util"){
            cout << "report-util command recognized. Doing something." << endl;
        }

        else if (command == "clear"){
            system("cls");
            printHeader();
        }

        else if (command == "exit"){
            return 0;
        }

        else{
            cout << "Invalid Command" << endl;
        }
    }

    return 0;
}