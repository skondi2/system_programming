/**
 * Shell
 * CS 241 - Spring 2020
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include "sstring.h"

typedef struct process {
    char *command;
    pid_t pid;
} process;

// updated111
FILE* historyHelper(char* histFileName);
FILE* fileHelper(char* filename);
int executeHist(char* histCpy, vector* commandHist);
void readHistFile(vector* commandHistory);
char* getHistPath(char* argv[], int argc, int* histArg, char* histPath);
char* getFilePath(char* argv[], int argc, int* fileArg, char* filePath);
int executeCd(char* lineCommand, int isAndOperator, int isOrOperator);
int executeLine(char* lineCommand, int isFile, vector* commandHist, int andOperator, int orOperator,
    int isBackground, int redirect);
int executeN(vector* commandHist, char* nCommand);
int executePrefix(vector* commandHist, char* prefix);
void ctrlc_handler();
void writeToHistFile(vector* commandHist, char* historyPath);
void cleanup(int isHist, char* histPath, vector* commandHist);
void implementPS();
//void freeing(char* lineCpy, char* prefix, char* cdCpy, char* histCpy, char* n, char* currDir);
process_info* gatherPsInfo(char* command, int pidNum);
void waitOnChild();
static char* foregroundCommand;
static pid_t foregroundPid;
static vector* backgroundPids;
static vector* backgroundCommands;

/**
* Helper function to append contents to commandHist to file
**/
void writeToHistFile(vector* commandHist, char* historyPath) {
   // write contents of commandHist into history file
   //printf("history path : %s\n", historyPath);
   //printf("strcmp history path: %d\n", strcmp(historyPath, "help.txt"));
   FILE* histFile = historyHelper(historyPath);
   if (histFile == NULL) {
       print_history_file_error();
       return;
   }
   
   size_t vecSize = vector_size(commandHist);
 
   for (size_t i = 0; i < vecSize; i++) {
       fprintf(histFile, "%s\n", (char*)vector_get(commandHist, i));
   }
 
   fclose(histFile);
}

void waitOnChild() {
    //printf("entering waitOnChild()\n");
    pid_t curr_pid;
    curr_pid =0;
    while ( (curr_pid = waitpid((pid_t) (-1), 0, WNOHANG)) > 0) {
        //printf("these are the current pids : %d\n", (int)curr_pid);
        size_t j = 0;
        while (j < vector_size(backgroundPids)) {
            int back_pid = *(int*)vector_get(backgroundPids, j);
            //printf("curr pid : &d\n", (int)curr_pid);
            //printf("background")
            if ((pid_t)back_pid == curr_pid) {
                //"erasing this pid: %d\n", back_pid);
                vector_erase(backgroundPids, j);
                vector_erase(backgroundCommands, j);
                break;
            } else {
                j++; 
            }
        }
        //curr_pid = waitpid((pid_t) (-1), 0, WNOHANG);
    }
}

process_info* gatherPsInfo(char* command, int pidNum) {
    process_info* processInfo = malloc(sizeof(process_info));

    processInfo->pid = pidNum;
    processInfo->command = command;

    char statusProcPath[69];
    sprintf(statusProcPath, "/proc/%d/status", pidNum);
    FILE* statusProc = fopen(statusProcPath, "r");
    if (statusProc== NULL) {
        //printf("failed to open proc status for %d\n", pidNum);
        return NULL;
    }

    char statProcPath[69];
    sprintf(statProcPath, "/proc/%d/stat", pidNum);
    FILE* statProc = fopen(statProcPath, "r");
    if (statProc == NULL) {
        //printf("failed to open proc pid stat\n");
        return NULL;
    }

    FILE* statNoPid = fopen("/proc/stat", "r");
    if (statNoPid == NULL) {
        //printf("failed to open proc stat\n");
        return NULL;
    }

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, statusProc)) != -1) {
        if (strstr(line, "Threads:") != NULL) {
            long int numThreads = 0;
            sscanf(line, "Threads: %ld", &numThreads);
            //printf("numThreads is : %ld\n", numThreads);
            processInfo->nthreads = numThreads;
        }
        if (strstr(line, "VmSize:") != NULL) {
            unsigned long int vmSize = 0;
            sscanf(line, "VmSize : %lu", &vmSize);
            //printf("vmSize is : %lu\n", vmSize);
            processInfo->vsize = vmSize;
            //printf("vmSize in kilo : %lu\n", vmSize/1024);
        }
        if (strstr(line, "State:") != NULL) {
            char state = '0';
            sscanf(line, "State : %c", &state);
            //printf("state is : %c\n", state);
            processInfo->state = state;
        }
    }
    if (line) {
        free(line);
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    char * line1 = NULL;
    size_t len1 = 0;
    ssize_t read1;

    read1 = getline(&line1, &len1, statProc);

    sstring* statLine = cstr_to_sstring(line1);
    vector* statVec = sstring_split(statLine, ' ');
    void* h = (vector_get(statVec, 21));
    unsigned long long hh = (unsigned long long)(atol(h));
    unsigned long long startTime = hh / ticks_per_sec;

    void* b = (vector_get(statVec, 13));
    unsigned long bb = atol(b);
    unsigned long userTime = bb / ticks_per_sec;

    void* a = (vector_get(statVec, 14));
    unsigned long aa = atol(a);
    unsigned long kernelTime = aa / ticks_per_sec;

    if (line1) {
        free(line1);
    }

    // convert user time into minutes
    unsigned long userMin = userTime/60;
    unsigned long userSec = userTime - userMin*60; 

    // convert kernel time into minutes
    unsigned long kernelMin = kernelTime/60;
    unsigned long kernelSec = kernelTime - kernelMin*60; 
        
    char bufferTime[68];
    execution_time_to_string(bufferTime, 68, kernelMin+userMin, kernelSec+userSec);
    processInfo->time_str = bufferTime;

    char * line2 = NULL;
    size_t len2 = 0;
    ssize_t read2;
    unsigned long long bTime = 0;
    while ((read2 = getline(&line2, &len2, statNoPid)) != -1) {
        if (strstr(line2, "btime") != NULL) {
            sscanf(line2, "btime %llu", &bTime);
        }
    }
    if (line2) {
        free(line2);
    }

    unsigned long long combinedTime = startTime + bTime;
    time_t startTime_T = (time_t)(combinedTime);
    struct tm* startTime_struct = localtime(&startTime_T);
    char buffer[68];
    time_struct_to_string(buffer, 68, startTime_struct);
    processInfo->start_str = buffer;

    fclose(statNoPid);
    fclose(statProc);
    fclose(statusProc);

    return processInfo;
}

void implementPS() {
    // print for ./shell
    // print for currently running children of ./shell
    print_process_info_header();
    print_process_info(gatherPsInfo("./shell", getpid()));

    for (size_t i = 0; i < vector_size(backgroundPids); i++) {
        char* backgroundCommand = (char*)vector_get(backgroundCommands, i);
        int backgroundPid = *(int*)vector_get(backgroundPids, i);
        //printf("passing in command : %s\n", backgroundCommand);
        //printf("passing in pid : %d\n", backgroundPid);
        //if (gatherPsInfo(backgroundCommand, backgroundPid) == NULL) return;
        if (waitpid(backgroundPid, 0, WNOHANG) == 0) {
            print_process_info(gatherPsInfo(backgroundCommand, backgroundPid));
        }
    }
}

/**
 * Signal handler to check for foreground processes and 
 * kill it
 * 
 * */
void ctrlc_handler() {
    
    // kill the current foreground process with SIGINT
    if (foregroundPid != -1) {
        kill(foregroundPid, SIGINT); // OR IS THIS JUST getpid()???
    }
    fflush(stdout);
}

/**
 * Helper function to kill all stopped or running background
 * child processes 
 **/
void cleanup(int isHist, char* histPath, vector* commandHist) {
   if (isHist) {
       writeToHistFile(commandHist, histPath);
   }
   // kill currently running background processes
   for (size_t j = 0; j < vector_size(backgroundPids); j++) {
       int pid_int = *(int*)vector_get(backgroundPids, j);
       pid_t pid_kill = (pid_t)(pid_int);
       kill(pid_kill, SIGTERM);
   }
   //printf("foreground pid : %d\n", (int)foregroundPid);
   if ((int)foregroundPid != -1) {
    kill(foregroundPid, SIGTERM);
   }
   vector_clear(backgroundPids);
   vector_clear(backgroundCommands);
}

/**
 * Helper function to execute cd command
 * 
 * Return 0 == command wasn't cd
 * Return 1 == continue to next while iteration
 * 
 **/
int executeCd(char* lineCommand, int isAndOperator, int isOrOperator) {
    //printf("entering cd here : --%s-- !\n", lineCommand);
    //printf("length of command : %lu\n", strlen(lineCommand));
    //printf("and : %d, or: %d\n", isAndOperator, isOrOperator);
    char* currDir = get_current_dir_name(); // its malloced
    char* lineCpy = NULL;
    if (isOrOperator || isAndOperator) {
        lineCpy = malloc(sizeof(char) * (strlen(lineCommand) + 2));
        lineCpy = strcpy(lineCpy, lineCommand);
        lineCpy[strlen(lineCommand)] = '\n';
    } else {
        lineCpy = malloc(sizeof(char) * (strlen(lineCommand) + 1));
        lineCpy = strcpy(lineCpy, lineCommand); // manipulate lineCpy
    }

    //printf("line copy is here: --%s-- !\n", lineCpy);

    char* token = strtok(lineCpy, " ");
    
    if (strcmp(token, "cd") == 0) {
        //printf("entering cd here\n");
        
        char* path = get_full_path(strtok(NULL, " "));
        size_t pathLen = strlen(path);
    
        if (strcmp(path, "..\n") == 0) {
            size_t dirLen = strlen(currDir);
            for (size_t i = dirLen - 1; ; i--) {
                if (currDir[i] == '/') {
                    currDir[i] = '\0';
                }
            }
            int switchedDir = chdir(currDir);
            if (switchedDir == -1) {
                //printf("2\n");
                print_no_directory(path);
                // need to continue to next while --> couldn't switch
                //printf("couldn't change paths.. return 2\n");
                /*if (currDir) free(currDir);
                if (lineCpy) free(lineCpy);
                if (path) free(path);*/
                return 0;
            } else {
                /*if (lineCpy) free(lineCpy);
                if (currDir) free(currDir);
                if (path) free(path);*/
                return 1;
            }
        } else {
            path[pathLen - 1] = '\0';
            int switchedDir = chdir(path);
            //printf("whyyyy : %d\n", switchedDir);
            if (switchedDir == -1) {
                //printf("yeah think i found it \n");
                print_no_directory(path);
                // need to continue to next while --> couldn't switch
                //printf("couldn't change paths.. return 2\n");
                /*if (lineCpy) free(lineCpy);
                if (currDir) free(currDir);
                if (path) free(path);*/
                return 0;
            } else {
                /*if (lineCpy) free(lineCpy);
                if (path)free(path);
                if (currDir) free(currDir);*/
                return 1;
            }
        }
    }

    if (strcmp(token, "cd\n") == 0) {
        //printf("what here?!?!\n");
        print_no_directory(currDir);
        //free(lineCpy);
        // need to continue to next while --> couldn't switch
        /*if (lineCpy) free(lineCpy);
        if (currDir) free(currDir);*/
        
        return 0;
    }
    //printf("current direct --> wasn't CD: %s\n", get_current_dir_name());
    /*if (lineCpy) free(lineCpy);
    if (currDir) free(currDir);*/
    
    return 2; // command wasn't cd --> still do executeLine
}

void readHistFile(vector* commandHistory) {
    size_t vecSize = vector_size(commandHistory);
    for (size_t i = 0; i < vecSize; i++) {
        print_history_line(i+1, (char*)vector_get(commandHistory, i));
    }
}

int executePrefix(vector* commandHist, char* prefix) {
    char* lineCpy = malloc(sizeof(char) * (strlen(prefix) + 1));
    lineCpy = strcpy(lineCpy, prefix); // manipulate lineCpy

    if (*lineCpy == '!') {
        int yes = -1;
        int execute = 0;
        size_t vecSize = vector_size(commandHist);
        if (strcmp(lineCpy, "!\n") == 0) {
            //printf("edge case: desired : %s\n", (char*)vector_get(commandHist, vecSize-1));
            yes = vecSize - 1;
            execute = 1;
            //return 1;
        }
        if (strcmp(lineCpy, "!history\n") == 0) {
            /*if (lineCpy) {
                free(lineCpy);
                lineCpy = NULL;
            }*/
            return 0;
        }
        if (!execute) {
            lineCpy++;
            //int cont = 0;
            for (int i = (int)vecSize - 1; i>=0 ; i--) {
                //printf("i here : %d\n", i);
                char* indiv = (char*)vector_get(commandHist, i);
                //printf("indiv : %s\n", indiv);
                size_t indivSize = strlen(indiv);
                int cont = 0;
                int breaking = 0;
                size_t min = indivSize;
                if (strlen(lineCpy) - 1 < min) {
                    min = strlen(lineCpy) - 1;
                }
                //printf("this is min : %zu\n", min);
                for (size_t j = 0; j < min; j++) {
                    //printf("indiv[j] : %c\n", indiv[j]);
                    //printf("lineCpy[j] : %c\n", lineCpy[j]);
                    if (indiv[j] != lineCpy[j]) {
                        cont = 1;
                        break;
                    }

                    if (indiv[j] == lineCpy[j] && j == min - 1) {
                        //printf("reached here .. bitch: %d\n", i);
                        yes = i;
                        execute = 1;
                        breaking = 1;
                        break;
                        //desired = indiv;
                    }
                }
                if (cont) {
                    continue;
                }
                if (breaking) break;
            }
        }
        if (yes == -1) {
            print_no_history_match();
            /*if (lineCpy) {
                free(lineCpy);
                lineCpy = NULL;
            }*/
            return 2; // continue to next iteration 
        }
        //printf("yes : %d\n", yes);
        //printf("desired : %s\n", (char*)vector_get(commandHist, yes));
        if (execute) {
            fflush(stdout);
            vector_push_back(commandHist, (char*)vector_get(commandHist, yes));

            pid_t pid = fork();

            if (pid < 0) { // fork failed
                print_fork_failed();
                /*if (lineCpy) {
                    free(lineCpy);
                }*/
                return 2;  // continue to next while iteration
            } else if (pid > 0) { // parent process
                int status;
                int waitSuccess = waitpid(pid, &status, 0);
                if (waitSuccess == -1) {
                    print_wait_failed();
                    /*if (lineCpy) {
                        free(lineCpy);
                    }*/      
                    return 2; // continue to next while iteration
                }
            } else { // child process

                //print_prompt(get_current_dir_name(), getpid());
                printf("%s", (char*)vector_get(commandHist, yes));
                print_command_executed(getpid());
                //vector_push_back(commandHist, (char*)vector_get(commandHist, yes));

                //printf("length of command : %lu\n", strlen(lineCopy));
            //printf("about to exec this command : --%s--\n", (char*)vector_get(commandHist, yes));
            //lineCpy[strlen(lineCpy)-1] = '\0';
            char* bl = (char*)vector_get(commandHist, yes);
            if (bl[strlen(bl)-1] == '\n') {
                bl[strlen(bl)-1] = '\0';
            }
            //printf("about to exec this command NOW: --%s--\n", bl);

            sstring* argument = cstr_to_sstring(bl);
            vector* argVec = sstring_split(argument, ' ');
            char* argArray[vector_size(argVec)+1];
            for (size_t i = 0; i < vector_size(argVec); i++) {
                argArray[i] = (char*)vector_get(argVec, i);
            }
            if (!strcmp(argArray[vector_size(argVec) - 1], "")) {
                argArray[vector_size(argVec)-1] = NULL;
            } else {
                argArray[vector_size(argVec)] = NULL;
            }

            int execSuccess =  execvp(argArray[0], argArray);

                //printf("about to exec this command : %s\n", (char*)vector_get(commandHist, yes));
                //int execSuccess =  execl("/bin/sh", "/bin/sh", "-c", (char*)vector_get(commandHist, yes), 0);
                if (execSuccess == -1) {
                    //printf("inside prefix\n");
                    print_exec_failed(/*(char*)vector_get(commandHist, yes)*/ bl);
                    //vector_erase(commandHist, yes); should i erase...?
                    // free(lineCpy); --> should I free here?
                    return 2; // continue to next while iteration
                }
            }
            /*if (lineCpy) {
                free(lineCpy);
                lineCpy = NULL;
            }*/
            return 2;
        }
        /*if (lineCpy) {
            free(lineCpy);
            lineCpy = NULL;
        }*/
        return 1;
    }
    /*if (lineCpy) {
        free(lineCpy);
        lineCpy = NULL;
    }*/
    return 0;
}

int executeN(vector* commandHist, char* nCommand) {
    char* lineCpy = malloc(sizeof(char) * (strlen(nCommand) + 1));
    lineCpy = strcpy(lineCpy, nCommand); // manipulate lineCpy

    //printf("first char of lineCpy : %c\n", *lineCpy);
    if (*lineCpy == '#') {
        //printf("second char of lineCpy : %s\n", (lineCpy+1));
        int y = atoi(lineCpy+1);
        //printf("y is : %d\n", y);
        if (y >= (int)vector_size(commandHist) || y < 0) {
            print_invalid_index();
            /*if (lineCpy) {
                free(lineCpy);
            }*/
            return 0;
        } else {
            print_command((char*)vector_get(commandHist, y));
            vector_push_back(commandHist, (char*)vector_get(commandHist, y));
            //fflush(stdout);
            pid_t pid = fork();

            if (pid < 0) { // fork failed
                print_fork_failed();
                return 2;  // continue to next while iteration
            } else if (pid > 0) { // parent process
                int status;
                int waitSuccess = waitpid(pid, &status, 0);
                if (waitSuccess == -1) {
                    print_wait_failed();
                    return 2; // continue to next while iteration
                }
            } else { // child process
                //print_prompt(get_current_dir_name(), getpid());
                //printf("%s", lineCommand);
                print_command_executed(getpid());

                //printf("length of command : %lu\n", strlen(lineCopy));
                char* boo = (char*)vector_get(commandHist, y);
            //printf("about to exec this command : --%s--\n", boo);
            //lineCpy[strlen(lineCpy)-1] = '\0';
            if (boo[strlen(boo)-1] == '\n') {
                boo[strlen(boo)-1] = '\0';
            }
            //printf("about to exec this command NOW: --%s--\n", boo);

            sstring* argument = cstr_to_sstring(boo);
            vector* argVec = sstring_split(argument, ' ');
            char* argArray[vector_size(argVec)+1];
            for (size_t i = 0; i < vector_size(argVec); i++) {
                argArray[i] = (char*)vector_get(argVec, i);
            }
            if (!strcmp(argArray[vector_size(argVec) - 1], "")) {
                argArray[vector_size(argVec)-1] = NULL;
            } else {
                argArray[vector_size(argVec)] = NULL;
            }

            int execSuccess =  execvp(argArray[0], argArray);
                
                //printf("about to exec this command : %s\n", lineCommand);
                //int execSuccess =  execl("/bin/sh", "/bin/sh", "-c", (char*)vector_get(commandHist, y), 0);
                if (execSuccess == -1) {
                    //printf("inside n\n");
                    print_exec_failed((char*)vector_get(commandHist, y));
                    exit(1);
                    //return 2; // continue to next while iteration
                }
            }
            //print_command_executed(getpid());
            return 1;
        }
    }
    /*if (lineCpy) {
        free(lineCpy);
    }*/
    return 0;
}

int executeHist(char* histCpy, vector* commandHist) {
    char* lineCpy = malloc(sizeof(char) * (strlen(histCpy) + 1));
    lineCpy = strcpy(lineCpy, histCpy); // manipulate lineCpy
    //printf("lineCpy : %s\n", lineCpy);
    if (strcmp(lineCpy, "!history\n") == 0) {
        readHistFile(commandHist);
        /*if (lineCpy) {
            free(lineCpy);
        }*/
        return 1; // command was !history --> was successful!
    }

    /*if (lineCpy) {
        free(lineCpy);
    }*/
    return 0; // command wasn't !history --> still do executeLine
}

/*void freeing(char* lineCpy, char* prefix, char* cdCpy, char* histCpy, char* n, char* currDir) {
    if (prefix) free(prefix);
    if (cdCpy) free(cdCpy);
    if (lineCpy) free(lineCpy);
    if (histCpy) free(histCpy);
    if (n) free(n);
    if (currDir) free(currDir);

    return;
}*/

/**
 * Helper function to call exec on lineCommand parameter
 * 
 * Return 0 to continue to next while iteration and not push
 * Return 1 to push into vector
 **/
int executeLine(char* lineCommand, int isFile, vector* commandHist, int andOperator, int orOperator,
    int isBackground, int redirect) {
    //printf("line being executed : %s\n", lineCommand);
    if (strcmp(lineCommand, "\n") == 0) {
        return 0;
    }
    
    char* currDir = get_current_dir_name();
    if (currDir == NULL) {
        print_no_directory(currDir);
        //free(currDir);
        return 0; // directory is wrong --> continue to next while iteration
    }

    if (strcmp(lineCommand, "ps\n") == 0) {
        return 0;
    }

    char* lineCopy = malloc(sizeof(char) * (strlen(lineCommand) + 1));
    char* cdCpy = malloc(sizeof(char) * (strlen(lineCommand) + 1));
    char* histCpy = malloc(sizeof(char) * (strlen(lineCommand) + 1));
    char* n = malloc(sizeof(char) * (strlen(lineCommand) + 1));
    char* prefix = malloc(sizeof(char) * (strlen(lineCommand) + 1));
    cdCpy = strcpy(cdCpy, lineCommand);
    histCpy = strcpy(histCpy, lineCommand);
    n = strcpy(n, lineCommand);
    prefix = strcpy(prefix, lineCommand);
    lineCopy = strcpy(lineCopy, lineCommand);

    //printf("copy of executeLine's lineCommand : %s\n", commandCpy);

    int isCD = executeCd(cdCpy, andOperator, orOperator);
    //printf("isCD : %d\n", isCD);
    int isHist = executeHist(histCpy, commandHist);
    int isN = executeN(commandHist, n);
    int isPrefix = executePrefix(commandHist, prefix);

    if (isPrefix == 1 || isPrefix == 2) {
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 0; // executed, continue to next while iteration
    }

    if (isN == 1 || isN == 2) { // command was #n
        // free everything else too when i return in this function
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 0; // executed, continue to next while iteration
    }
    if (isHist == 1) { // command was !history
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 0; // executed, continue to next while iteration
    }
    if ((isCD == 1 && andOperator ) || (isCD == 0 && orOperator)) {
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 1; // execute 2nd command
    }
    if ((isCD == 1 && orOperator) || (isCD == 0 && andOperator)) {
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 0; // continue to next while iteration
    }
    
    if (isCD == 0) {
        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
        return 0; // continue to next while iteration
    }

    if (isCD == 2 && isN == 0 && isHist == 0 && isPrefix == 0) { // command is external
        //printf("entering exec\n");
        fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) { // fork failed
            print_fork_failed();
            //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
            return 0;  // continue to next while iteration
        } else if (pid > 0) { // parent process
            //if (!isFile) {
                if (isBackground) {
                    //printf("what is linecommand : %s\n", lineCommand);
                    vector_push_back(backgroundCommands, lineCommand);
                    int back_pid = (int)pid;
                    vector_push_back(backgroundPids, &back_pid);
                    //printf("pushing into pid vector : %d\n", back_pid);
                    //printf("size of pid vector : %lu\n", vector_size(backgroundPids));
                } else { // it's a foreground
                    //printf("setting the foreground command to : %s\n", lineCommand);
                    //printf("setting foreground pid : %d\n", pid);
                    foregroundPid = pid;
                    foregroundCommand = lineCommand;
                }
            //}
            if (!isBackground) {
                int status;
                int waitSuccess = waitpid(pid, &status, 0);
                if (waitSuccess == -1) {
                    print_wait_failed();
                    //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
                    return 0; // continue to next while iteration
                }
                if (status == 0) { // executed correctly
                    print_command_executed(pid);
                    if (andOperator) {
                        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
                        return 1; // execute 2nd command
                    }
                    if (orOperator) {
                        //printf("returning 0 at correct place\n");
                        //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
                        return 0; // continue to next while iteration
                    }
                    //foregroundPid = pid;
                    //foregroundCommand = lineCommand;      
                } else { // failed
                    //printf("what is linecommand : %s\n", lineCommand);
                    //vector_push_back(backgroundCommands, lineCommand);
                    //int back_pid = (int)pid;
                    //vector_push_back(backgroundPids, &back_pid);
                    //printf("pushing into pid vector : %d\n", back_pid);
                    //printf("size of pid vector : %lu\n", vector_size(backgroundPids));
                    //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);

                    if (!orOperator) return 0;
                    return 1;
                }
            } else {
                int waitSuccess = waitpid(pid, NULL, WNOHANG);
                if (waitSuccess == -1) {
                    print_wait_failed();
                    //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
                    return 0; // continue to next while iteration
                }
                print_command_executed(pid);
            }
        } else { // child process
            //printf("length of command : %lu\n", strlen(lineCopy));
            //printf("about to exec this command : --%s--\n", lineCopy);
            if (lineCopy[strlen(lineCopy)-1] == '\n') {
                lineCopy[strlen(lineCopy)-1] = '\0';
            }
            //printf("about to exec this command NOW: --%s--\n", lineCopy);

            sstring* argument = cstr_to_sstring(lineCopy);
            vector* argVec = sstring_split(argument, ' ');
            char* argArray[vector_size(argVec)+1];
            for (size_t i = 0; i < vector_size(argVec); i++) {
                argArray[i] = (char*)vector_get(argVec, i);
            }
            if (!strcmp(argArray[vector_size(argVec) - 1], "")) {
                argArray[vector_size(argVec)-1] = NULL;
            } else {
                argArray[vector_size(argVec)] = NULL;
            }
            int execSuccess = 0;
            if (redirect) {
                execSuccess =  execl("/bin/sh", "/bin/sh", "-c", lineCopy, 0);
            } else {
                execSuccess =  execvp(argArray[0], argArray);
            }
            //int execSuccess =  execl("/bin/sh", "/bin/sh", "-c", lineCopy, 0);
            if (execSuccess < 0) {
                //printf("inside executeLine!\n");
                //print_invalid_command(lineCopy);
                print_exec_failed(lineCopy);
                //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
                if (orOperator) {
                    return 1; // if failed the first, then still execute 2nd command
                } if (andOperator) {
                    return 0; // continue to next while iteration
                    // TRY PUTTING EXIT CODE HERE
                }
                exit(1);
            }
        }
    }
    //freeing(lineCopy, prefix, cdCpy, histCpy, n, currDir);
    //printf("or is it printing here\n");
    return 0; // executed command, continue to next iteration
}

/**
 * Helper function to determine if history file is valid
 * 
 * Return NULL if opening file failed
 * Return FILE* on success
 **/
FILE* historyHelper(char* histFileName) {
    FILE* histFile = fopen(histFileName, "a+");
    return histFile;
}

/**
 * Helper function to determine if -f file is valid
 * 
 * Return NULL if opening file failed
 * Return FILE* on success
 **/
FILE* fileHelper(char* filename) {
    FILE* file = fopen(filename, "r");
    return file;
}

/**
 * Helper function to traverse through arguments past ./shell
 * 
 * Set histArg = 1 if argv contains -h and update histPath
 * Set fileArg = 1 if argv contains -f and update filePath
 * If argv contains invalid arguments, return NULL
 * 
 * Return NULL if unsuccessful, histPath if successful optional arguments inputted
 **/
char* getHistPath(char* argv[], int argc, int* histArg, char* histPath) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            *histArg = 1;
            if (i + 1 >= argc) {
                print_usage();
                return NULL;
            }
            if (i+1 != argc) {
                histPath = argv[i+1];
                if (strcmp(histPath, "-f") == 0) {
                    print_usage();
                    return NULL; 
                }
            } else {
                print_usage();
                return NULL;
            }
            FILE* successH = historyHelper(histPath);
            if (successH == NULL) {
                print_history_file_error();
                return NULL;
            }
        }
        if (strcmp(argv[i], "-f") != 0 && strcmp(argv[i], "-h") != 0) {
            if (i != 1 && (strcmp(argv[i-1], "-f") != 0 && strcmp(argv[i-1], "-h")!= 0)) {
                print_usage();
                return NULL;
            }
        }
    }
    return histPath;
}

char* getFilePath(char* argv[], int argc, int* fileArg, char* filePath) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            *fileArg = 1;
            if (i + 1 >= argc) {
                print_usage();
                return NULL;
            }
            filePath = (argv[i+1]);
            FILE* successF = fileHelper(filePath);
            if (successF == NULL) {
                print_script_file_error();
                return NULL;
            }
        }
        // possibly do this in another for loop
        // if the phrase before was -h or -f, this phrase could be the file path
        if (strcmp(argv[i], "-f") != 0 && strcmp(argv[i], "-h") != 0) {
            if (i != 1 && (strcmp(argv[i-1], "-f") != 0 && strcmp(argv[i-1], "-h"))) {
                print_usage();
                return NULL;
            }
        }
    }
    return filePath;
}

int shell(int argc, char *argv[]) {
    // TODO: This is the entry point for your shell.
    // store command history
    //signal(SIGINT, ctrlc_handler);
    vector* commandHist = string_vector_create(); 

    // analyze argv commands
    if (argc > 5) {
        print_usage();
        exit(0);
    }
    int historyArg = 0;
    int fileArg = 0;
    char* historyPath = NULL;
    char* filePath = NULL;
    historyPath = getHistPath(argv, argc, &historyArg, historyPath);
    filePath = getFilePath(argv, argc, &fileArg, filePath);

    if (historyArg == 1 && historyPath == NULL) {
        print_usage();
        cleanup(historyArg, historyPath, commandHist);
        exit(0);
    }
    if (fileArg == 1 && filePath == NULL) {
        print_usage();
        cleanup(historyArg, historyPath, commandHist);
        exit(0);
    }
    if (fileArg == 0 && historyArg == 0 && argc != 1) {
        print_usage();
        cleanup(historyArg, historyPath, commandHist);
        exit(0);
    }
    if ((fileArg == 1 && historyArg == 0 && argc != 3) || (fileArg == 0 && historyArg == 1 && argc != 3)) {
        print_usage();
        cleanup(historyArg, historyPath, commandHist);
        exit(0);
    }

    backgroundPids = int_vector_create();
    backgroundCommands = string_vector_create();
    foregroundCommand = malloc(sizeof(char) * 100);
    signal(SIGINT, ctrlc_handler);
    foregroundPid = -1;

    while (1) {

        //foregroundCommand = malloc(sizeof(char) * 100);
        //foregroundPid = -1;
        
        waitOnChild();

        // check for Ctrl-D / EOF
        if (feof(stdin)) {
            cleanup(historyArg, historyPath, commandHist);
            printf("%s\n", ""); // should i do this?
            exit(0);
        }

        // check for Ctrl-C 
        //signal(SIGINT, ctrlc_handler);

        // execute file commands
        if (fileArg) {
            char * line = NULL;
            size_t len = 0;
            ssize_t read;
            FILE* filePtr = fileHelper(filePath);
            if (filePtr == NULL) {
                print_script_file_error();
                //printf("error in script file\n");
                continue; // for failed
            }

            while ((read = getline(&line, &len, filePtr)) != -1) {
                //printf("Retrieved line of length %zu:\n", read);
                //printf("%s\n", line);
                print_prompt(get_current_dir_name(), getpid());
                printf("%s", line);
                vector_push_back(commandHist, line);

                executeLine(line, fileArg, commandHist, 0, 0, 0, 0);
            }
            fclose(filePtr);
            /*if (line) {
                free(line);
            }*/
            cleanup(historyArg, historyPath, commandHist); // kill background processes 
            //fclose(filePtr);
            exit(0); // reached EOF 
        }

        char* currDir = get_current_dir_name();
        if (currDir == NULL) {
            print_no_directory(currDir);
            //if (currDir) free(currDir);
            continue;
        }
        print_prompt(get_current_dir_name(), getpid());
        int killPid = 0;
        int contPid = 0;
        int stopPid = 0;
        
        if (!fileArg) {
            // get stdin input
            char * line = NULL;
            size_t len = 0;
            ssize_t read;
            if ((read = getline(&line, &len, stdin)) != -1) {
                //printf("reading in this line --%s--\n", line);

                if (strcmp(line, "exit\n") == 0) {
                    //printf("entering cleanup\n");
                    //printf("bitch forgeounr pid : %d\n", foregroundPid);
                    cleanup(historyArg, historyPath, commandHist); // kill background processes
                    //printf("should exit with status 0\n");
                    //kill(getpid(), 0);
                    exit(0);// reached exit keyword
                    //break;
                    //return 1;
                }

                sstring* argument = cstr_to_sstring(line);
                vector* argVec = sstring_split(argument, ' ');
                char* argArray[vector_size(argVec)+1];
                for (size_t i = 0; i < vector_size(argVec); i++) {
                    argArray[i] = (char*)vector_get(argVec, i);
                }

                if (strcmp(line, "exit\n") != 0) {

                    if (strcmp(argArray[0], "kill\n") == 0) {
                        print_invalid_command(line);
                        continue;
                    } else if (strcmp(argArray[0], "kill") == 0) {
                        if (vector_size(argVec) > 2) {
                            print_invalid_command(line);
                            continue;
                        }
                        killPid = 1;
                    } else if (strcmp(argArray[0], "cont\n") == 0) {
                        print_invalid_command(line);
                        continue;
                    } else if (strcmp(argArray[0], "cont") == 0) {
                        if (vector_size(argVec) > 2) {
                            print_invalid_command(line);
                            continue;
                        }
                        contPid = 1;
                    } else if (strcmp(argArray[0], "stop\n") == 0) {
                        print_invalid_command(line);
                        continue;
                    } else if (strcmp(argArray[0], "stop") == 0) {
                        if (vector_size(argVec) > 2) {
                            print_invalid_command(line);
                            continue;
                        }
                        stopPid = 1;
                        //printf("set stopPid to 1\n");
                    }

                    if (killPid) {
                        pid_t pid = (pid_t)(atoi(argArray[1]));
                        if (pid == 0) {
                            print_no_process_found(pid);
                            continue;
                        }
                        int killSuccess = kill(pid, SIGTERM);

                        if (killSuccess == -1) {
                            print_no_process_found(pid);
                            continue;
                        } 
                        print_killed_process(pid, line);
                        size_t j = 0;
                        while (j < vector_size(backgroundPids)) {
                            int back_pid = *(int*)vector_get(backgroundPids, j);
                            //printf("curr pid : &d\n", (int)curr_pid);
                            //printf("background")
                            if ((pid_t)back_pid == pid) {
                                //printf("erasing this pid: %d\n", back_pid);
                                vector_erase(backgroundPids, j);
                                vector_erase(backgroundCommands, j);
                                break;
                            } else {
                                j++; 
                            }
                        }
                        continue;
                    }

                    if (contPid) {
                        pid_t pid = (pid_t)(atoi(argArray[1]));
                        if (pid == 0) {
                            print_no_process_found(pid);
                            continue;
                        }
                        int contSuccess = kill(pid, SIGCONT);
                        if (contSuccess == -1) {
                            print_no_process_found(pid);
                            continue;
                        } 
                        print_continued_process(pid, line);
                        continue;
                    }

                    if (stopPid) {
                        //printf("entering stop\n");
                        pid_t pid = (pid_t)(atoi(argArray[1]));
                        if (pid == 0) {
                            print_no_process_found(pid);
                            continue;
                        }
                        int stopSuccess = kill(pid, SIGSTOP);
                        if (stopSuccess == -1) {
                            print_no_process_found(pid);
                            continue;
                        } 
                        print_stopped_process(pid, line);
                        continue;
                    }

                    //printf("Retrieved line of length %zu:\n", read);
                    //printf("--%s--\n", line);
                    int orPosition = -1;
                    int andPosition = -1;
                    int sepPosition = -1;
                    int amperPosition = -1;
                    int redirOutPos = -1;
                    int redirAppPos = -1;
                    int redirInPos = -1;

                    if (strstr(line, "&&") != NULL) {
                        andPosition = (int)(strstr(line, "&&") - line);
                    }
                    if (strstr(line, "||") != NULL) {
                        orPosition = (int)(strstr(line, "||") - line);
                    }
                    if (strstr(line, ";") != NULL) {
                        sepPosition = (int)(strchr(line, ';') - line);
                    }
                    if (strstr(line, "&") != NULL) {
                        amperPosition = (int) (strchr(line, '&') - line);
                    }
                    if (strstr(line, ">") != NULL) {
                        redirOutPos = (int)(strstr(line, ">") - line);
                    }
                    if (strstr(line, ">>") != NULL) {
                        redirAppPos = (int)(strstr(line, ">>") - line);
                    }
                    if (strstr(line, "<") != NULL) {
                        redirInPos = (int)(strstr(line, "<") - line);
                    }
                    //printf("and position : %d\n", andPosition);
                    //printf("or position : %d\n", orPosition);
                    //printf("sep position : %d\n", sepPosition);

                    int containsAnd = (andPosition != -1);
                    int containsOr = (orPosition != -1);
                    int containsSep = (sepPosition != -1);
                    int containsAmp = (amperPosition == ((int)strlen(line) - 2));
                    int redirectOutput = redirOutPos != -1;
                    int redirectAppend = redirAppPos != -1;
                    int redirectInput = redirInPos != -1;

                    if (historyArg && (containsAnd || containsOr || containsSep)) {
                        vector_push_back(commandHist, line);
                    }

                    char* otherHalf = line;
                    if (containsAnd) {
                        otherHalf = &line[andPosition+3];
                        //printf("here is the other half : %s\n", otherHalf);
                        line[andPosition-1] = '\0';
                        //printf("are you doing this null char?\n");
                    } else if (containsOr) {
                        otherHalf = &line[orPosition+3];
                        line[orPosition-1] = '\0';
                    } else if (containsSep) {
                        otherHalf = &line[sepPosition+2];
                        line[sepPosition] = '\0';
                    } else if (containsAmp) {
                        otherHalf[amperPosition - 1] = '\0';
                        executeLine(otherHalf, fileArg, commandHist, 0, 0, containsAmp, 0);
                        continue;
                    }
                    if (containsAnd) {
                        int success = executeLine(line, fileArg, commandHist, containsAnd, containsOr, 
                            containsAmp, 0);
                        //printf("what is the && success : %d\n", success);
                        if (success == 1) { 
                            executeLine(otherHalf, fileArg, commandHist, containsAnd, containsOr,
                                containsAmp, 0);
                        } else {
                            /*if (currDir) {
                                free(currDir);
                            }*/
                            continue;
                        }
                    } 
                    if (containsOr) {
                        int success = executeLine(line, fileArg, commandHist, containsAnd, containsOr,
                            containsAmp, 0);
                        //printf("what is the || success : %d\n", success);
                        if (success == 1) {
                            executeLine(otherHalf, fileArg, commandHist, containsAnd, containsOr,
                                containsAmp, 0);
                        } else {
                            /*if (currDir) {
                                free(currDir);
                            }*/
                            continue;
                        }
                    }

                    if (containsSep) {
                        executeLine(line, fileArg, commandHist, 1, 0, 0, 0);
                        executeLine(otherHalf, fileArg, commandHist, 1, 0, 0, 0);
                    }
                    
                    else if (!containsOr && !containsAnd && !containsSep) {
                        executeLine(line, fileArg, commandHist, 0, 0, containsAmp, 
                            (redirectOutput || redirectInput || redirectAppend ));
                        if (strcmp(line, "ps\n") == 0) {
                            // implement PS
                            implementPS();
                            continue;
                        }
                    }
                    if (!(containsAnd || containsOr || containsSep) && strcmp(line, "!history\n") != 0 
                        && *line != '#' && *line != '!') {
                        vector_push_back(commandHist, line);
                    }
                    
                }
            }
            /*if (line) {
                free(line);
            }
            if (currDir) {
                free(currDir);
            }*/
        }
        /*if (foregroundCommand) {
            free(foregroundCommand); // CAUSING MEMORY ERROR
        }*/
    }
    return 0;
} 