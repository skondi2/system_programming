/**
 * Utilities Unleashed
 * CS 241 - Spring 2020
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include "format.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
// partners : nehap2 & jcolla3

int main(int argc, char *argv[]) {
    if (argc < 2) {
        void print_env_usage();
    }
    
    pid_t pid = fork();
    
    if (pid < 0) { // fork failed
        print_fork_failed();
        exit(1);
    } else if (pid > 0) {  // we are in parent
        int status;
        waitpid(pid, &status, 0);
    } else { // we are in the child process
        int start = 0;
        for (int i = 1; i < argc; i++) {
            //printf("in for loop\n");
            if (strcmp(argv[i], "--") != 0) {
                //printf("enterining here\n");
                char* name = strtok(argv[i], "=");
                char* value = strtok(NULL, "=");

                size_t nameLen = strlen(name);
                size_t valueLen = strlen(value);
                for (size_t i = 0; i < nameLen; i++) { // check name is valid input
                    if (!isalpha(name[i]) && !isdigit(name[i]) && name[i]!='_') {
                        print_env_usage();
                        return 0;
                    }
                }
                for (size_t j = 0; j < valueLen; j++) { // check value is valid input
                    if (value[j] == '%' && j != 0) {
                        print_env_usage();
                        return 0;
                    }
                    if (!isalpha(value[j]) && !isdigit(value[j]) && value[j]!='_' && (j == 0 && value[j]!='%')) {
                        print_env_usage();
                        return 0;
                    }
                }
                //printf("name: %s\n", name);
                //printf("value : %s\n", value);

                //if (getenv(value) == NULL) {
                    //print_env_usage();
                //}
                if (value[0] == '%') {
                    value += 1;
                    if (getenv(value) == NULL) {
                        print_env_usage();
                    }
                    setenv(name, getenv(value), 1);
                } else {
                    setenv(name, value, 1);
                }

            } else {
                start = i+1;
                break;
            }
            if (i == argc-1) {
                print_env_usage();
                return 0;
            }
        }
        int status = execvp(argv[start], &argv[start]); // run argv[] with exec
        if (status == -1) {
            print_exec_failed();
        }
        exit(1);
    }
    return 0;
}

