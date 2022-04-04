/** 
 * mapreduce
 * CS 241 - Spring 2020
 */
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>          
#include <unistd.h>

 #include <sys/types.h>
  #include <sys/wait.h>

int main(int argc, char **argv) {
    
    char* split_num_string = argv[argc - 1];
    int split_num = 0;
    sscanf(split_num_string, "%d", &split_num);
    
    // Create an input pipe for each mapper.
    int mapper_fds[split_num][2];
    
    for (int i = 0; i < split_num; i++) {
        int fds[2];
        pipe2(fds, O_CLOEXEC);

        mapper_fds[i][0] = fds[0];
        mapper_fds[i][1] = fds[1];

        descriptors_add(fds[0]);
        descriptors_add(fds[1]);
    }
    
    // Create one input pipe for the reducer.
    int reduce_fds[2];
    pipe2(reduce_fds, O_CLOEXEC);
    descriptors_add(reduce_fds[0]);
    descriptors_add(reduce_fds[1]);

    // Open the output file.
    char filename[64];
    sprintf(filename, "%s", argv[2]);
    FILE* output = fopen(filename, "w");
    int output_fd = fileno(output);

    //fprintf(stderr, "bdsfh");

    // Start a splitter process for each mapper
    // helpful reference : https://stackoverflow.com/questions/2605130/redirecting-exec-output-to-a-buffer-or-file
    // http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
    pid_t pids[split_num];
    for (int i = 0; i < split_num; i++) {
        pid_t pid = fork();

        if (pid == 0) { // child
            
            dup2(mapper_fds[i][1], 1); // stdout of process is mapper write-end
            close(mapper_fds[i][1]);

            char input1[2];
            sprintf(input1, "%d", split_num);

            char input2[2];
            sprintf(input2, "%d", i);

            char* inputs[5];
            inputs[0] = "./splitter";
            inputs[1] = argv[1];
            inputs[2] = input1;
            inputs[3] = input2;
            inputs[4] = NULL;
            execvp(inputs[0], inputs); // call exec on splitter
            
        } else if (pid > 0) { // parent
            pids[i] = pid;
        }
    }
    
    //fprintf(stderr, "dddd\n");
    pid_t childs[split_num];
    for (int i = 0; i < split_num; i++) {
        
        pid_t child = fork();
        if (child == 0) { // child
            dup2(mapper_fds[i][0], 0); // stdin of process is mapper read-end
            close(mapper_fds[i][0]);

            dup2(reduce_fds[1], 1); // stdout of process is reducer write-end
            if (i == split_num - 1) {
                close(reduce_fds[1]);
            }

            char* inputs[2];
            inputs[0] = argv[3];
            inputs[1] = NULL;
            execvp(inputs[0], inputs); // call exec on mapper
        } else if (child > 0) { // parent
            childs[i] = child;
        }
    }

    //fprintf(stderr, "ffffff\n");
    // Start the reducer process.
    pid_t fork1 = fork();
    if (fork1 == 0) { // child

        dup2(reduce_fds[0], 0); // stdin of process is reducer read-end
        close(reduce_fds[0]);

        dup2(output_fd, 1); // stdout of process is output file

        char* inputs[2];
        inputs[0] = argv[4];
        inputs[1] = NULL;
        execvp(inputs[0], inputs); // call exec on reducer
    } else if (fork1 > 0) { // parent
       
    }

    fclose(output);
    close(output_fd);
    descriptors_closeall();
    descriptors_destroy();

    // Print nonzero subprocess exit codes.
    //fprintf(stderr, "ccccc\n");
    for (int i = 0; i < split_num; i++) {
        int status1;
        waitpid(pids[i], &status1, 0);

        int exit = WEXITSTATUS(status1);
        if (exit != 0) {
            print_nonzero_exit_status("./splitter", exit);
        }
    }

    //fprintf(stderr, "eeeeee\n");
    for (int i = 0; i < split_num; i++) {
        int status2;
        waitpid(childs[i], &status2, 0);

        int exit = WEXITSTATUS(status2);
        if (exit != 0) {
            print_nonzero_exit_status(argv[3], exit);
        }
    }

    //fprintf(stderr, "ggggg\n");
    int status3;
    waitpid(fork1, &status3, 0);
        
    int exit = WEXITSTATUS(status3);
    if (exit != 0) {
        print_nonzero_exit_status(argv[4], exit);
    }

    //fprintf(stderr, "eeeee\n");

    // Count the number of lines in the output file.
    print_num_lines(argv[2]);

    //fprintf(stderr, "dfsdkhfskd\n");

    return 0;
}