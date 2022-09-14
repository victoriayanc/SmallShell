#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;
extern SmallShell& smash;
#define NO_CMD_RUNNING -1

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    if (smash.fg_cmd_pid != NO_CMD_RUNNING){
        if (killpg(getpgid(smash.fg_cmd_pid), SIGKILL) == -1)
            cout << "signal handling failed" << endl;
        else
            cout << "smash: process " << smash.fg_cmd_pid << " was killed" << endl;
    }
}

void ctrlZHandler(int sig_num) {
	  cout << "smash: got ctrl-Z" << endl;
    if (smash.fg_cmd_pid != NO_CMD_RUNNING){
        if (killpg(getpgid(smash.fg_cmd_pid), SIGSTOP) == -1)
            cout << "signal handling failed" << endl;
        else {
            cout << "smash: process " << smash.fg_cmd_pid << " was stopped" << endl;
        }
    }
}

void alarmHandler(int sig_num) {
    cout << "smash: got an alarm" << endl;

    AlarmCommand* cmd = smash.removeFinishedAlarm();
    while(cmd != NULL){
        int wait_res = waitpid(cmd -> pid, 0, WNOHANG);
        if (wait_res == -1 || wait_res == cmd -> pid){
            delete(cmd);
            cmd = smash.removeFinishedAlarm();
            continue;
        }

        if (kill(cmd -> pid, SIGKILL) == -1){   
            cout << "signal handling failed" << endl;
        }
        else{
            cout << "smash: " << cmd -> cmd_line << " timed out!" << endl;
            delete(cmd);
        }
        cmd = smash.removeFinishedAlarm();
    }
    smash.setNewAlarm();
}

