/**
 * Utilities Unleashed
 * CS 241 - Spring 2020
 */
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include "format.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

int main(int argc, char *argv[]) { 
    if (argc < 2) {
        print_time_usage();
        return 0;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) { // fork failed
        print_fork_failed();
        exit(1);
    } 
    if (pid > 0) { // we are in the parent process
        clockid_t clk_id = CLOCK_MONOTONIC;
        int status=0;
        struct timespec start, end;
        int startSuccess = clock_gettime(CLOCK_MONOTONIC, &start);
        if (startSuccess) exit(1);
        
        waitpid(pid, &status, 0);

        int endSuccess = clock_gettime(clk_id, &end);
        if (endSuccess) exit(1);
        if (WEXITSTATUS(status) != 1) {
            double elapsedTime;
            elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/10000000000.0;
            display_results(argv, elapsedTime);
        }
    } else { // we are in the child process
        int f = execvp(argv[1], argv+1); // run argv[] with exec
        if (f) {
            print_exec_failed();
        }
    }
    
    return 0;
}
