#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"


extern SmallShell& smash;
extern bool global_quit;
using namespace std;
const std::string WHITESPACE = " \n\r\t\f\v";

#define NO_CMD_RUNNING -1
#define JOB_DOES_NOT_EXIT -1
#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}
string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}
int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;
  FUNC_EXIT()
}
bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}
void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}
string getParameter(string cmd_line, int i){
    istringstream iss(cmd_line);
    string word;
    int j = -1;
    while (iss >> word){
        j += 1;
        if (j == i)
            break;
    }
    if (j != i)
        return "";
    return word;
}


AlarmCommand::AlarmCommand(const string cmd_line): Command(cmd_line){
    pid = -1;
    sec_arg = getParameter(cmd_line, 1);
    starting_time = time(nullptr);
    inner_cmd_str = "";
    string arg;
    int i = 2;
    do {
        arg = getParameter(cmd_line, i++);
        if (arg != "")
            inner_cmd_str += arg + " ";
    }
    while(arg != "");
}
ExternalCommand::ExternalCommand(const string cmd_line): Command(cmd_line){}
ExternalCommand::~ExternalCommand(){}
ChangeDirCommand::ChangeDirCommand(const string cmd_line):BuiltInCommand(cmd_line) {
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);
    
    arg1 = getParameter(cmd_line_without_bs, 1);
    arg2 = getParameter(cmd_line_without_bs, 2);
}
ChangePromptCommand::ChangePromptCommand(const string cmd_line):BuiltInCommand(cmd_line) {
    string cmd_line_without_bs = cmd_line;
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);

    arg = getParameter(cmd_line_without_bs, 1);
}
KillCommand::KillCommand(const string cmd_line) : BuiltInCommand(cmd_line){
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);


    signal_num_arg = getParameter(cmd_line_without_bs, 1);
    job_id_arg = getParameter(cmd_line_without_bs, 2);
    arg3 = getParameter(cmd_line_without_bs, 3);
}

ForegroundCommand::ForegroundCommand(const string cmd_line):BuiltInCommand(cmd_line){
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);

    this -> job_id_arg = getParameter(cmd_line_without_bs, 1);
    this -> arg2 = getParameter(cmd_line_without_bs, 2);
}
BackgroundCommand::BackgroundCommand(const string cmd_line):BuiltInCommand(cmd_line){
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);

    this -> job_id_arg = getParameter(cmd_line_without_bs, 1);
    this -> arg2 = getParameter(cmd_line_without_bs, 2);
}
QuitCommand::QuitCommand(const string cmd_line):BuiltInCommand(cmd_line){
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);

    to_kill = false;
    int i=1;
    string par;
    do {
        par = getParameter(cmd_line_without_bs, i++);
        if (par == "kill")
            to_kill = true;
    }
    while(par!="");
    global_quit = true;
}
PipeCommand::PipeCommand(string cmd_line): Command(cmd_line) {
    string copy_copy = cmd_line;
    if (cmd_line.find("|&") != string::npos) {
        fd = 2;
        command1 = cmd_line.erase(cmd_line.find("|&"));
        command2 = copy_copy.erase(0, copy_copy.find("|&") + 2);
    } else {
        fd = 1;
        command1 = cmd_line.erase(cmd_line.find("|"));
        command2 = copy_copy.erase(0, copy_copy.find("|") + 1);
    }
    while (command2.find("&") != string::npos)
        command2.erase(command2.find("&"), 1);
}
RedirectionCommand::RedirectionCommand(const string cmd_line):Command(cmd_line){
    if (cmd_line.find(">>") != string::npos) {
        append = true;
        string copy_cmd = cmd_line;
        command = copy_cmd.erase(cmd_line.find(">>"));
        copy_cmd = cmd_line;
        path = getParameter(copy_cmd.erase(0,copy_cmd.find(">>") + 2) ,0);
    } else {
        append = false;
        string copy_cmd = cmd_line;
        command = copy_cmd.erase(cmd_line.find(">"));
        copy_cmd = cmd_line;
        path = getParameter(copy_cmd.erase(0,copy_cmd.find(">") + 1), 0);
    }
    while (path.find("&") != string::npos)
        path.erase(path.find("&"), 1);

  //  char resolved_path[PATH_MAX];
  //  realpath(path.c_str(),resolved_path);
  //  path = resolved_path;

}
CopyCommand::CopyCommand(string cmd_line) : Command(cmd_line) {
    string cmd_line_without_bs = cmd_line;
    
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);

    old_file_path = getParameter(cmd_line_without_bs, 1);
    old_path_to_print = old_file_path;
    if (old_file_path != ""){
        char resolved_path_1[PATH_MAX];
        realpath(old_file_path.c_str(),resolved_path_1);
        this->resolved_path_1 = resolved_path_1;
    }

    //    cout << "old path is: " << old_file_path << endl;

    new_file_path = getParameter(cmd_line_without_bs,2);
    new_path_to_print = new_file_path;
    if (new_file_path != ""){
        char resolved_path_2[PATH_MAX];
        realpath(new_file_path.c_str(),resolved_path_2);
        this->resolved_path_2 = resolved_path_2;
    }

       // cout << "new path is: " << new_file_path << endl;
}





//aux func
time_t AlarmCommand::terminationTime(){
    return starting_time + stoi(sec_arg);
}
void AlarmCommand::execute(){
    smash.setNewAlarm();
    string cmd_name = getParameter(inner_cmd_str, 0);

    if (isExternal(cmd_name) || cmd_name == "cp"){
        Command* cmd_to_joblist = new AlarmCommand(cmd_line);
        int child_pid = fork();
        if (child_pid == 0) {
            setpgrp();
            Command* cmd = smash.CreateCommand(inner_cmd_str);
            cmd->execute();
            delete(cmd);
            exit(0);
        }
        else if(child_pid > 0) {
            this -> pid = child_pid;
            if(_isBackgroundComamnd(inner_cmd_str.c_str())) {
                smash.addJob(cmd_to_joblist, child_pid, false);
            }
            else{
                int status;
                smash.fg_cmd_pid = child_pid;
                smash.fg_cmd = cmd_to_joblist;
                waitpid(child_pid, &status, WUNTRACED);
                if (WIFSTOPPED(status)){
                    smash.addJob(cmd_to_joblist, smash.fg_cmd_pid, true);
                }
                else {
                    delete(cmd_to_joblist);
                }
                smash.fg_cmd = NULL;
                smash.fg_cmd_pid = NO_CMD_RUNNING;
            }   
        }
        else {
            perror("smash error: fork failed");
        }
    }
    //if the cmd is built in command
    else {
        Command* cmd = smash.CreateCommand(inner_cmd_str);
        cmd->execute();
        delete cmd;
    }
    //print_v(smash.alarms_cmd_vec);
}
void ChangePromptCommand::execute() {
    arg == "" ? smash.setPrompt("smash") : smash.setPrompt(arg);
}
void ShowPidCommand::execute(){
    cout << "smash pid is " << smash.smash_pid << endl;
}
void pwdCommand::execute(){
	char buf[PATH_MAX];
	cout << getcwd(buf, PATH_MAX) << endl;
}
void ChangeDirCommand::execute() {
    if (arg2 != "") {
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }
    if (arg1 == "") return;

    string curr_dir = smash.getCurrDir();
    string prev_dir = smash.getPrevDir();
    string next_dir;
    const char* c_next_dir;

    if (arg1 == "-") {
        if (prev_dir == "") {
            cerr << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
        c_next_dir = prev_dir.c_str();
        next_dir = string(c_next_dir);
    } 
    else {
        c_next_dir = arg1.c_str();
        next_dir = arg1;
    }

    if (chdir(c_next_dir) == FAILURE) {
        perror("smash error: chdir failed");
        return;
    }

    smash.setPrevDir(curr_dir);

    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    smash.setCurrDir(buf);
}
//aux func
bool isInteger(const std::string & s){
   if(s.empty() || (!isdigit(s[0]) && (s[0] != '-'))) return false;

   char * p;
   strtol(s.c_str(), &p, 10);
   return (*p == 0);
}
void KillCommand::execute(){
    if ((arg3 != "") || (signal_num_arg == "") ||
        (!isInteger(job_id_arg) ||
        (!isInteger(signal_num_arg.substr(1, signal_num_arg.size() - 1))))) {
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
    }

    int signal_num = stoi(signal_num_arg.substr(1, signal_num_arg.size() - 1));
    int job_id = stoi(job_id_arg);
    int job_pid = smash.job_list.getJobEntryPID(smash.job_list.getJobById(job_id));

    //if (signal_num > 31 || signal_num < 1){
    //   cerr << "smash error: kill: invalid arguments" << endl;
    //    return;
    //}
    if (smash.job_list.getJobEntryPID(smash.job_list.getJobById(atoi(job_id_arg.c_str()))) == JOB_DOES_NOT_EXIT) {
        cerr << "smash error: kill: job-id "<< job_id_arg <<" does not exist" << endl;
        return;
    }
    
    if (killpg(getpgid(job_pid), signal_num) == -1){
        perror("smash error: kill failed");
    }
    else{
        cout << "signal number " << signal_num << " was sent to pid " << job_pid << endl;
    }



}
void JobsCommand::execute(){
  smash.printJobs();
}

void ForegroundCommand::execute(){
    if (arg2 != "" || (job_id_arg!="" && !isInteger(job_id_arg))){
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    if (job_id_arg == "" && smash.job_list.isEmpty()){
        cerr << "smash error: fg: jobs list is empty" << endl;
        return;
    }
    if (job_id_arg != "" && smash.job_list.getJobEntryPID(smash.job_list.getJobById(atoi(job_id_arg.c_str()))) == JOB_DOES_NOT_EXIT) {
        cerr << "smash error: fg: job-id "<< job_id_arg <<" does not exist" << endl;
        return;
    }
    int arg_to_int = atoi(job_id_arg.c_str());
    int job_pid, job_id;
    if (job_id_arg == ""){
        job_pid = smash.job_list.getJobEntryPID(smash.job_list.getLastJob(false));
        job_id  = smash.job_list.getJobEntryJID(smash.job_list.getLastJob(false));
    }
    else {
        job_pid = smash.job_list.getJobEntryPID(smash.job_list.getJobById(arg_to_int));
        job_id  = smash.job_list.getJobEntryJID(smash.job_list.getJobById(arg_to_int));
    }
    
    
    int status;
    Command* cmd = smash.job_list.removeJobById(job_id);
    cout << cmd ->cmd_line << " : " << job_pid << endl;
    smash.fg_cmd_pid = job_pid;
    smash.fg_cmd = cmd;
    if (killpg(getpgid(job_pid), SIGCONT) == -1){
        perror("smash error: kill failed");
        smash.job_list.addJob(cmd, job_pid, job_id, true);
    }
    else {
        waitpid(job_pid, &status, WUNTRACED);
        if (WIFSTOPPED(status)){
            smash.job_list.addJob(cmd, job_pid, job_id, true);
        }
        else {
            delete(cmd);
        }
    }
    smash.fg_cmd_pid = NO_CMD_RUNNING;
    smash.fg_cmd = NULL;
}
void BackgroundCommand::execute(){
    if (arg2 != "" || (job_id_arg!="" && !isInteger(job_id_arg))){
        cerr << "smash error: bg: invalid arguments" << endl;
        return;
    }
    if (job_id_arg != "" && smash.job_list.getJobEntryPID(smash.job_list.getJobById(atoi(job_id_arg.c_str()))) == JOB_DOES_NOT_EXIT) {
        cerr << "smash error: bg: job-id "<< job_id_arg <<" does not exist" << endl;
        return;
    }
    if (job_id_arg != "" && smash.job_list.jobIsRunning(atoi(job_id_arg.c_str()))){
        cerr << "smash error: bg: job-id "<< job_id_arg << " is already running in the background" << endl;
        return;
    }
    if (job_id_arg == "" && (smash.job_list.isEmpty() || smash.job_list.getLastJob(true)==NULL)){
        cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
        return;
    }
    int arg_to_int = atoi(job_id_arg.c_str());
    int job_pid, job_id;
    if (job_id_arg == ""){
        job_pid = smash.job_list.getJobEntryPID(smash.job_list.getLastJob(true));
        job_id  = smash.job_list.getJobEntryJID(smash.job_list.getLastJob(true));
    }
    else {
        job_pid = smash.job_list.getJobEntryPID(smash.job_list.getJobById(arg_to_int));
        job_id  = smash.job_list.getJobEntryJID(smash.job_list.getJobById(arg_to_int));
    }


    Command* cmd = smash.job_list.getJobEntryCMD(smash.job_list.getJobById(job_id));
    cout << cmd ->cmd_line << " : " << job_pid << endl;
    if (killpg(getpgid(job_pid), SIGCONT) == -1)
        perror("smash error: kill failed");
    else
        smash.job_list.setIsStopped(smash.job_list.getJobById(job_id), false);

}
void QuitCommand::execute(){
    if (to_kill)
        cout << "smash: sending SIGKILL signal to " << smash.job_list.jobs_vec.size() << " jobs:" << endl;
    smash.job_list.killAllJobs(to_kill);
}

void ExternalCommand::execute(){
    char* args[4];

    char* c_bash_str = new char[10];
    strcpy(c_bash_str,"/bin/bash");

    char* c_c = new char[3];
    strcpy(c_c,"-c");

    char* c_cmd = new char[cmd_line.size() + 1];
	strcpy(c_cmd, cmd_line.c_str());
	_removeBackgroundSign(c_cmd);
    //string cmd_without_bgs = c_cmd;

   // cmd_without_bgs = "\"" + cmd_without_bgs + "\"";
  //  const char* c_copy_cmd_without_bgs = cmd_without_bgs.c_str();
  //  char* c_cmd_without_bgs = new char[cmd_without_bgs.size() + 1];
  //  strcpy(c_cmd_without_bgs, c_copy_cmd_without_bgs);

    args[0] = c_bash_str;
    args[1] = c_c;
    args[2] = c_cmd;
    args[3] = NULL;

    if (execv(args[0], args) == FAILURE){
        perror("smash error: execv failed");
    }
}
void PipeCommand::execute() {
    int my_pipe[2];
    if (pipe(my_pipe) == FAILURE) {
        perror("smash error: pipe failed");
        return;
    }

    int pid = fork();
    if (pid > 0) {// dad writes
        int pid_child2 = fork();
        if (pid_child2 == 0){
            dup2(my_pipe[1],fd);
            close(my_pipe[0]);
            close(my_pipe[1]);
            Command* cmd = smash.CreateCommand(command1);
            cmd->execute();
            delete(cmd);
            close(fd);
            exit(0);
        }
        else if(pid_child2 > 0){
            close(my_pipe[0]);
            close(my_pipe[1]);
            waitpid(pid_child2, 0, 0);
            waitpid(pid, 0, 0);
            return;
        }
        else{
            perror("smash error: fork failed");
            return;
        }
    }
    else if (pid == 0) {// son reads
        dup2(my_pipe[0],0);
        close(my_pipe[0]);
        close(my_pipe[1]);
        Command* cmd = smash.CreateCommand(command2);
        cmd->execute();
        delete(cmd);
        close(0);
        exit(0);
    }
    else {
        perror("smash error: fork failed");
    }
}
void RedirectionCommand::execute() {
    Command* red_cmd_to_joblist = new RedirectionCommand(cmd_line);
    Command* inner_cmd = smash.CreateCommand(command);

    if(isExternal(getParameter(command, 0))) {
        int child_pid = fork();
        if (child_pid == 0) {
            close(1);
            int fd = FAILURE;
            const char* c_path = path.c_str();

            if (append) {
                fd = open(c_path, O_CREAT|O_WRONLY|O_APPEND, 0666);
            } else {
                fd = open(c_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
            }

            if (fd == FAILURE) {
                //cerr << "from open error number is: "<< errno << endl;
                perror("smash error: open failed");
                exit(0);
            }


            setpgrp();
            inner_cmd->execute();
            delete(inner_cmd);
            close(fd);
            exit(0);
        }
        else if(child_pid > 0) {
            if(_isBackgroundComamnd(cmd_line.c_str())) {
                smash.addJob(red_cmd_to_joblist, child_pid, false);
            } 
            else {
                int status;
                smash.fg_cmd_pid = child_pid;
                smash.fg_cmd = red_cmd_to_joblist;
                waitpid(child_pid, &status, WUNTRACED);
                if (WIFSTOPPED(status)) {
                    smash.addJob(red_cmd_to_joblist, child_pid, true);
                } else {
                    delete (inner_cmd);
                    delete (red_cmd_to_joblist);
                }
                smash.fg_cmd = NULL;
                smash.fg_cmd_pid = NO_CMD_RUNNING;
            }
        }
        else {
            perror("smash error: fork failed");
        }
    }
    else {
        int monitor_stdout = dup(1);
        close(1);
        int fd = FAILURE;
        const char* c_path = path.c_str();
        if (append) {
            fd = open(c_path, O_CREAT|O_WRONLY|O_APPEND, 0666);
        } else {
            fd = open(c_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
        }
        if (fd == FAILURE) {
            dup(monitor_stdout);
            perror("smash error: open failed");
            return;
        }
        inner_cmd -> execute();
        delete(inner_cmd);
        close(1);
        dup(monitor_stdout);
    }

}
void CopyCommand::execute() {
    if (new_path_to_print == "" || old_path_to_print == ""){
        cerr << "smash error: copy: invalid arguments" << endl;
        return;
    }

    const char* c_old_path = old_file_path.c_str();
    int old_fd = open(c_old_path, O_RDONLY);
    if (old_fd == FAILURE) {
        perror("smash error: open failed");
        return;
    }

    if (resolved_path_1 == resolved_path_2) {
        close(old_fd);
        cout << "smash: " << old_path_to_print << " was copied to " << new_path_to_print << endl;
        return;
    }

    const char* c_new_path = new_file_path.c_str();
    int new_fd = open(c_new_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if (new_fd == FAILURE) {
        close(old_fd);
        perror("smash error: open failed");
        return;
    }

    char buf[4096];
    int count_write = 0;
    int count_curr_write;

    int count_read = read(old_fd, buf, 4096);
    if (count_read == FAILURE) {
        perror("smash error: read failed");
        close(old_fd);
        close(new_fd);
        return;
    }
    if (count_read == 0) {
        close(new_fd);
        remove(c_new_path);
        open(c_new_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
        close(old_fd);
        cout << "smash: " << old_path_to_print << " was copied to " << new_path_to_print << endl;
        return;
    }

    while (count_read > 0) {
        while (count_write < count_read) {

            count_curr_write = write(new_fd, buf, count_read);
            if (count_curr_write == FAILURE) {
                perror("smash error: write failed");
                close(old_fd);
                close(new_fd);
                break;
            }
            count_write += count_curr_write;
        }

        count_write = 0;
        if (count_curr_write == FAILURE){
            break;
        }
        count_read = read(old_fd, buf, 4096);
        if (count_read == FAILURE) {
            perror("smash error: read failed");
            close(old_fd);
            close(new_fd);
            break;
        }
    }

    close(old_fd);
    close(new_fd);
    cout << "smash: " << old_path_to_print << " was copied to " << new_path_to_print << endl;
}




bool isExternal(const string command){
	if (command == "chprompt" || command == "showpid"
        || command == "pwd" || command == "cd"
        || command == "jobs" || command == "kill"
        || command == "fg" || command == "bg"
        ||command == "kill" || command == "timeout")
		return false;
	return true;
}
Command* SmallShell::CreateCommand(string cmd_line) {
    string cmd_line_without_bs = cmd_line;
    while (cmd_line_without_bs.find("&") != string::npos)
        cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);
    string command = getParameter(cmd_line_without_bs,0);
	if (command == "chprompt") {
		return new ChangePromptCommand(cmd_line);
	}
	else if (command == "showpid") {
		return new ShowPidCommand(cmd_line);
	}
	else if (command == "pwd") {
		return new pwdCommand(cmd_line);
	}
	else if (command == "cd") {
		return new ChangeDirCommand(cmd_line);
	}
	else if (command == "jobs") {
		return new JobsCommand(cmd_line);
	}
	else if (command == "kill") {
		return new KillCommand(cmd_line);
	}
	else if (command == "fg") {
		return new ForegroundCommand(cmd_line);
	}
	else if (command == "bg") {
		return new BackgroundCommand(cmd_line);
	}
	else if (command == "quit") {
		return new QuitCommand(cmd_line);
	}
    else if (command == "cp") {
		return new CopyCommand(cmd_line);
	}
	else {
		return new ExternalCommand(cmd_line);
	}
	return NULL;
}
void SmallShell::executeRedirectionCommand(const string cmd_line) {
    Command *cmd = new RedirectionCommand(cmd_line);
    cmd -> execute();
    delete(cmd);
}
void SmallShell::executePipeCommand(const string cmd_line){
    Command* cmd = new PipeCommand(cmd_line);
    int pid = fork();
    if (pid > 0) {
        if(!_isBackgroundComamnd(cmd_line.c_str())){
            int status;
            this -> fg_cmd_pid = pid;
            this -> fg_cmd = cmd;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)){
                this -> addJob(this -> fg_cmd, this -> fg_cmd_pid, true);
            }
            else {
                delete(cmd);
            }
            this -> fg_cmd = NULL;
            this -> fg_cmd_pid = NO_CMD_RUNNING;
        }
        else {
            smash.addJob(cmd, pid, false);
        }
    }
    else if (pid == 0) {
        setpgrp();
        cmd->execute();
        delete (cmd);
        exit(0);
    }
    else
        perror("smash error: fork failed");
}
void SmallShell::executeExternalCommand(const string cmd_line){
    Command *cmd = CreateCommand(cmd_line);
    int child_pid = fork();
    if (child_pid == 0) {
        setpgrp();
        cmd->execute();
        delete(cmd);
        exit(0);
    }
    else if(child_pid > 0) {
        if(_isBackgroundComamnd(cmd_line.c_str())) {
	          smash.addJob(cmd, child_pid, false);
        }
        else{
            int status;
            this -> fg_cmd_pid = child_pid;
            this -> fg_cmd = cmd;
	        waitpid(child_pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)){
                this -> addJob(this -> fg_cmd, this -> fg_cmd_pid, true);
            }
            else {
                delete(cmd);
            }
            this -> fg_cmd = NULL;
            this -> fg_cmd_pid = NO_CMD_RUNNING;
        }   
    }
    else {
        perror("smash error: fork failed");
    }
}
void SmallShell::executeBuiltInCommand(const string cmd_line){
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    delete (cmd);
}
void SmallShell::executeCopyCommand(const string cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    int pid = fork();
    if (pid == 0) {
        setpgrp();
        cmd->execute();
        delete(cmd);
        exit(0);
    }
    else if(pid > 0) {
        if(_isBackgroundComamnd(cmd_line.c_str())) {
            smash.addJob(cmd, pid, false);
        }
        else{
            int status;
            this -> fg_cmd_pid = pid;
            this -> fg_cmd = cmd;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)){
                this -> addJob(this -> fg_cmd, this -> fg_cmd_pid, true);
            }
            else {
                delete(cmd);
            }
            this -> fg_cmd = NULL;
            this -> fg_cmd_pid = NO_CMD_RUNNING;
        }
    }
    else {
        perror("smash error: fork failed");
    }
}
void SmallShell::executeAlarmCommand(const string cmd_line){
    AlarmCommand* cmd = new AlarmCommand(cmd_line);
    if(cmd->sec_arg == "" || !isInteger(cmd->sec_arg) || stoi(cmd->sec_arg) <= 0 || cmd->inner_cmd_str == ""){
        cerr << "smash error: timeout: invalid arguments" << endl;
        delete(cmd);
        return;
    }
    smash.alarms_cmd_vec.push_back(cmd);
    cmd -> execute();
}

void SmallShell::executeCommand(const string cmd_line) {
    if (getParameter(cmd_line, 0) == "timeout")
        executeAlarmCommand(cmd_line);
    else if (cmd_line.find("|") != string::npos)
        executePipeCommand(cmd_line);
    else if (cmd_line.find(">") != string::npos)
        executeRedirectionCommand(cmd_line);
    else if (getParameter(cmd_line,0) == "cp")
        executeCopyCommand(cmd_line);
    else {
        string cmd_line_without_bs = cmd_line;
        while (cmd_line_without_bs.find("&") != string::npos)
            cmd_line_without_bs.erase(cmd_line_without_bs.find("&"), 1);
        string command = getParameter(cmd_line_without_bs,0);

        if (isExternal(command)){
            executeExternalCommand(cmd_line);
        }
        else {
            executeBuiltInCommand(cmd_line);
        }

    }

}
//
SmallShell::SmallShell() : prompt("smash"), prev_dir(""),
                           fg_cmd_pid(NO_CMD_RUNNING), fg_cmd(nullptr) {
    
    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    curr_dir = buf;
    this -> smash_pid = getpid();
}
SmallShell::~SmallShell() {
// TODO: add your implementation
}
string SmallShell::getPrompt() {
    return prompt;
}
void SmallShell::setPrompt(string str) {
    prompt = str;
}
string SmallShell::getPrevDir() {
    return prev_dir;
}
void SmallShell::setPrevDir(string prev) {
    prev_dir = prev;
}
string SmallShell::getCurrDir() {
    return curr_dir;
}
void SmallShell::setCurrDir(string curr) {
    curr_dir = curr;
}
void SmallShell::addJob(Command* cmd, int pid,bool is_stopped){
    job_list.addJob(cmd, pid, is_stopped);
}
void SmallShell::printJobs(){
    job_list.printJobsList();
}
void SmallShell::setNewAlarm(){
    int min = INT_MAX;
    for(auto x: smash.alarms_cmd_vec){
        if(difftime(x->terminationTime(), time(nullptr)) < min)
        {
            min = difftime(x->terminationTime(), time(nullptr));
        }
    }
    if (min != INT_MAX)
        alarm(min);
}
AlarmCommand* SmallShell::removeFinishedAlarm(){
    for(unsigned int i = 0; i < smash.alarms_cmd_vec.size(); i++){
        if(difftime(time(nullptr), smash.alarms_cmd_vec[i]->terminationTime()) == 0){
            AlarmCommand* cmd = smash.alarms_cmd_vec[i];
            smash.alarms_cmd_vec.erase(smash.alarms_cmd_vec.begin() + i);
            return cmd;
        }
    }
    return NULL;
}




JobsList::JobEntry::JobEntry(int pid, int job_id, bool is_stopped, Command* cmd){
    this->pid = pid;
    this->job_id = job_id;
    this->is_stopped = is_stopped;
    this->cmd = cmd;
    this->start_time = time(nullptr);
}
JobsList::JobsList() {
    jobs_vec = vector<JobEntry*>();
}
JobsList::~JobsList(){
    for (unsigned int i = 0; i < jobs_vec.size(); i++){
        delete(jobs_vec[i]->cmd);
        delete(jobs_vec[i]);
    }
}
int JobsList::getNewJobId(){
    if(this->isEmpty())
        return 1;
    this->sortList();
    reverse(jobs_vec.begin(),jobs_vec.end());
    return jobs_vec[0] -> job_id + 1;
}
void JobsList::addJob(Command* cmd, int pid, bool is_stopped){
    this -> removeFinishedJobs();
    if (this->getNewJobId()<1){
        cout << "you got negative JOB ID, you should not be here!!!";
        exit(0);
    }
    JobEntry* job = new JobEntry(pid, this->getNewJobId(), is_stopped, cmd);
    jobs_vec.push_back(job);
}
void JobsList::addJob(Command* cmd, int pid, int jobid, bool is_stopped){
    this -> removeFinishedJobs();
    if (this->getNewJobId()<1){
        cout << "you got negative JOB ID, you should not be here!!!";
        exit(0);
    }
    JobEntry* job = new JobEntry(pid, jobid, is_stopped, cmd);
    jobs_vec.push_back(job);
}
void JobsList::printJobsList(){
    this -> removeFinishedJobs();
    this -> sortList();
    for (JobEntry* job: jobs_vec){
        cout << "[" << job->job_id << "] " << job->cmd->cmd_line << " : " << job->pid;
        cout << " " << difftime(time(nullptr), job ->start_time) << " secs";
        if (job -> is_stopped)
            cout << " (stopped)";
        cout << endl;
    }

}
void JobsList::removeFinishedJobs(){
    for (unsigned int i = 0; i < jobs_vec.size(); i++){
        if (waitpid(jobs_vec[i]->pid, 0, WNOHANG) != 0){
            delete(jobs_vec[i]->cmd);
            delete(jobs_vec[i]);
            jobs_vec.erase(jobs_vec.begin() + i);
            i--;
        }
    }
}
void JobsList::killAllJobs(bool to_kill) {
    smash.job_list.sortList();
    for (unsigned int i = 0; i < jobs_vec.size();){
        if (to_kill){
            if (killpg(getpgid(jobs_vec[i]->pid), SIGKILL) == -1)
                cout << "signal handling failed" << endl;
            else {
                cout << jobs_vec[i]->pid << ": " << jobs_vec[i] -> cmd -> cmd_line << endl;
            }
        }
        delete(jobs_vec[i]->cmd);
        delete(jobs_vec[i]);
        jobs_vec.erase(jobs_vec.begin() + i);
    }
}

JobsList::JobEntry* JobsList::getJobById(int jid){
    for(unsigned int i = 0; i < jobs_vec.size(); i++){
        if (jobs_vec[i] -> job_id == jid)
            return jobs_vec[i];
    }
    return NULL;
}
JobsList::JobEntry* JobsList::getLastJob(bool stopped_job){
    if (jobs_vec.empty())
        return NULL;
    else{
        this -> sortList();
        for (int i = jobs_vec.size() - 1; i >= 0; i--){
            if (stopped_job){
                if (jobs_vec[i]->is_stopped){
                    return jobs_vec[i];
                }
            }
            else{
                return jobs_vec[i];
            }
        }
    }
    return NULL;
}
Command* JobsList::removeJobById(int jobId){
    for (unsigned int i = 0; i < jobs_vec.size(); i++){
        if (jobs_vec[i] -> job_id == jobId){
            Command* cmd = jobs_vec[i]->cmd;
            delete(jobs_vec[i]);
            jobs_vec.erase(jobs_vec.begin() + i);
            return cmd;
        }
    }
    cout << "jobID does not exist!" << endl;
    return NULL;
}

int JobsList::getJobEntryPID(JobsList::JobEntry* job){
    if (!job)
        return JOB_DOES_NOT_EXIT;
    else
        return job->pid;

}
int JobsList::getJobEntryJID(JobsList::JobEntry* job){
    if (!job)
        return JOB_DOES_NOT_EXIT;
    else
        return job->job_id;

}
Command* JobsList::getJobEntryCMD(JobsList::JobEntry* job){
    if (!job)
        return NULL;
    else
        return job->cmd;

}
bool JobsList::jobIsRunning(int jid){
    for(unsigned int i = 0; i < jobs_vec.size(); i++){
        if (jobs_vec[i]->job_id == jid)
            return (jobs_vec[i]->is_stopped == false);
    }
    return false;
}
void JobsList::setIsStopped(JobsList::JobEntry* job, bool is_stopped){
    job -> is_stopped = is_stopped;
}

bool JobsList::isEmpty(){
    return jobs_vec.empty();
}
void JobsList::sortList(){
    if (jobs_vec.empty())
        return;
    sort(jobs_vec.begin(), jobs_vec.end(), JobsList::job_cmp);
}
bool JobsList::job_cmp(JobEntry* j1, JobEntry* j2){
    return (j1->job_id < j2->job_id);
}