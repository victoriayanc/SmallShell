#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

using namespace std;
SmallShell& smash = SmallShell::getInstance();
bool global_quit = false;
int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    struct sigaction *act = new struct sigaction();
    act -> sa_flags = SA_RESTART;
    act -> sa_handler = alarmHandler;
    if(sigaction(SIGALRM, act, NULL)==-1) {
        perror("smash error: failed to set alarm handler");
    }




    while(true) {
        if (global_quit)
            return 0;
        cout << smash.getPrompt() << "> "; // TODO: change this (why?)
        string cmd_line;
        getline(cin, cmd_line);
        smash.job_list.removeFinishedJobs();
        if (cmd_line == "")
            continue;
        else {
            smash.executeCommand(cmd_line.c_str());
        }
    }
    return 0;
}