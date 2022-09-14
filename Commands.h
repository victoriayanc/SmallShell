#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <iostream>
#include <string>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <climits>
#include <stdlib.h>


using namespace std;
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#define FAILURE (-1)

class JobsList;
//return the i parameter from the cmd line
//if i == 0 the return value is the command's name
string getParameter(string cmd_line, int i);
bool isInteger(const std::string & s);
bool isExternal(const string command);


class Command {
public:
    const string cmd_line;
    Command(string cmd_line):cmd_line(cmd_line){};
    virtual ~Command(){};
    virtual void execute() = 0;
};
class BuiltInCommand : public Command {
    public:
        BuiltInCommand(const string cmd_line): Command(cmd_line){};
        virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
    public:
        ExternalCommand(const string cmd_line);
        virtual ~ExternalCommand();
        void execute() override;
};
class PipeCommand : public Command {
    int fd;
    string command1;
    string command2;
    public:
        PipeCommand(string cmd_line);
        virtual ~PipeCommand() {}
        void execute() override;
};
class RedirectionCommand : public Command {
    bool append;
    string path;
    string command;

    public:
        explicit RedirectionCommand(const string cmd_line);
        virtual ~RedirectionCommand() {}
        void execute() override;
};
class AlarmCommand : public Command {
    public:
        string inner_cmd_str;
        string sec_arg;
        time_t starting_time;
        int pid;

        time_t terminationTime();
        AlarmCommand (const string cmd_line);
        ~AlarmCommand(){}
        void execute() override;
        
};

class ChangePromptCommand : public BuiltInCommand {
    string arg;
public:
    ChangePromptCommand(const string cmd_line);
    virtual ~ChangePromptCommand(){}
    void execute() override;
};
class ChangeDirCommand : public BuiltInCommand {
    string arg1;
    string arg2;
    public:
        ChangeDirCommand(const string cmd_line);
        virtual ~ChangeDirCommand(){}
        void execute() override;
};
class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line):BuiltInCommand(cmd_line){};
    virtual ~GetCurrDirCommand(){}
    void execute() override;
};
class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const string cmd_line):BuiltInCommand(cmd_line){}
    virtual ~ShowPidCommand(){}
    void execute() override;
};
class pwdCommand : public BuiltInCommand {
public:
    pwdCommand(const string cmd_line):BuiltInCommand(cmd_line){}
    virtual ~pwdCommand() {}
    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
	    int pid;
	    int job_id;
	    bool is_stopped;
        Command* cmd;
        time_t start_time;
	    JobEntry(int pid, int job_id, bool is_stopped, Command* cmd);
    };

	vector <JobEntry*> jobs_vec;
	JobsList();
	~JobsList();
	void addJob(Command* cmd, int pid,bool is_stopped);
    void addJob(Command* cmd, int pid, int jobid, bool is_stopped);
	void printJobsList();
	void removeFinishedJobs();
	void killAllJobs(bool to_kill);

	JobEntry *getJobById(int jobId);
	JobEntry *getLastJob(bool stopped_job);
	Command* removeJobById(int jobId);

    int getJobEntryPID(JobsList::JobEntry* job);
    int getJobEntryJID(JobsList::JobEntry* job);
    Command* getJobEntryCMD(JobsList::JobEntry* job);
    bool jobIsRunning(int jid);
    void setIsStopped(JobsList::JobEntry* job, bool is_stopped);

    bool isEmpty();
private:
    int getNewJobId();
    static bool job_cmp(JobEntry* j1, JobEntry* j2);
    void sortList();
};
class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(string const cmd_line):BuiltInCommand(cmd_line){};
    virtual ~JobsCommand(){}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    string signal_num_arg;
    string job_id_arg;
    string arg3;
public:
    KillCommand(const string cmd_line);
    virtual ~KillCommand() {}
    void execute() override;
};
class ForegroundCommand : public BuiltInCommand {
    string job_id_arg;
    string arg2;
public:
    ForegroundCommand(const string cmd_line);
    virtual ~ForegroundCommand(){}
    void execute() override;
};
class BackgroundCommand : public BuiltInCommand {
    string job_id_arg;
    string arg2;
public:
    BackgroundCommand(const string cmd_line);
    virtual ~BackgroundCommand(){}
    void execute() override;
};
class QuitCommand : public BuiltInCommand {
    bool to_kill;
public:
    QuitCommand(const string cmd_line);
    virtual ~QuitCommand() {}
    void execute() override;
};

// TODO: should it really inhirit from BuiltInCommand ?
class CopyCommand : public Command {
	string old_file_path;
	string new_file_path;

    string old_path_to_print;
    string new_path_to_print;

    string resolved_path_1;
    string resolved_path_2;
public:
    CopyCommand(string cmd_line);
    virtual ~CopyCommand() {}
    void execute() override;
};

// TODO: add more classes if needed 
// maybe chprompt , timeout ?



class SmallShell {
    string prompt;
    string curr_dir;
    string prev_dir;

    public:
        JobsList job_list;
        vector<AlarmCommand*> alarms_cmd_vec;
        pid_t smash_pid;
        int fg_cmd_pid;
        Command* fg_cmd;
        SmallShell();
        Command *CreateCommand(const string cmd_line);
        SmallShell(SmallShell const&)      = delete; // disable copy ctor
        void operator=(SmallShell const&)  = delete; // disable = operator
        static SmallShell& getInstance() // make SmallShell singleton
            {
                static SmallShell instance; // Guaranteed to be destroyed.
                // Instantiated on first use.
                return instance;
            }
        ~SmallShell();
        void executeCommand(const string cmd_line);
        void changePrompt(string par);
        string getPrompt();
        string getPrevDir();
        string getCurrDir();
        void setPrompt(string str);
        void setPrevDir(string prev);
        void setCurrDir(string curr);
        void addJob(Command* cmd, int pid,bool isStopped);
        void printJobs();
        void setNewAlarm();
        AlarmCommand* removeFinishedAlarm();
    private:
        void executePipeCommand(const string cmd_line);
        void executeBuiltInCommand(const string cmd_line);
        void executeExternalCommand(const string cmd_line);
        void executeRedirectionCommand(const string cmd_line);
        void executeCopyCommand(const string cmd_line);
        void executeAlarmCommand(const string cmd_line);
};




/*
void print_v(vector<Command*> vec){
    for(Command* x: vec){
        cout << x->cmd_line << endl;
    }
}
*/



#endif //SMASH_COMMAND_H_




