#include <iostream>
#include <string>

using namespace std;

void printHeader(){
    cout << "***Placeholder***";
}

int main(){
    int flag = 1;
    string command;

    printHeader();

    while (flag==1){
        cout << "\nEnter Command: ";
        cin >> command;


        if (command == "initialize"){
            cout << "\nSystem Initializing";
        }

        else if (command == "screen"){
            cout << "\nDoing Screen";
        }

        else if (command == "scheduler-start"){
            cout << "\nStarting Scheduler";

        }

        else if (command == "scheduler-stop"){
            cout << "\nStopping Scheduler";

        }

        else if (command == "report-util"){
            cout << "\nReporting Utilities";

        }

        else if (command == "clear"){
            printHeader();
            system("cls");
        }

        else if (command == "exit"){
            return 0;
        }

        else {
            cout << "Invalid Command";
        }
    }

    return 0;

}
