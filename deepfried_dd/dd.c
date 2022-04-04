/**
 * Deepfried dd
 * CS 241 - Spring 2020
 */
#include "format.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <math.h>

// partner: prajnak2

static volatile int caught_signal = 0;
void sig_handler(int sig);

int main(int argc, char **argv) {
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    double start = (double)(tp.tv_sec + (tp.tv_nsec*(pow(10.0, -9.0)))); 

    char* input_file = NULL;
    char* output_file = NULL;
    int block_size = 512; // default
    size_t total_blocks_copied = -1;
    int input_skip = 0; // default
    int output_skip = 0; // default

    int c;
    while ((c = getopt(argc, argv, "iobcpk:")) != -1) {
        switch(c) {
            case 'i':
                input_file = argv[optind];
                break;
            case 'o':
                output_file = argv[optind];
                break;
            case 'b': 
                sscanf(argv[optind], "%d", &block_size); 
                break;
            case 'c':
                sscanf(argv[optind], "%zd", &total_blocks_copied); 
                break;
            case 'p':
                sscanf(argv[optind], "%d", &input_skip); 
                break;
            case 'k':
                sscanf(argv[optind-1], "%d", &output_skip); 
                break;
            case '?':
                exit(1);
        }
    }

    FILE* input_fd = stdin; // default
    if (input_file != NULL) {
        input_fd = fopen(input_file, "r");
        if (input_fd == NULL) {
            print_invalid_input(input_file);
            exit(1);
        }
    }
    FILE* output_fd = stdout; // default
    if (output_file != NULL) {
        output_fd = fopen(output_file, "r+");
        if (output_fd == NULL) {
            output_fd = fopen(output_file, "w+"); // if file doesn't exist
            if (output_fd == NULL) {
                print_invalid_output(output_file);
                exit(1);
            }
        }
    }

    if (input_skip && input_fd != stdin) {
        fseek(input_fd, (long)input_skip, SEEK_SET);
    }
    if (output_skip && output_fd != stdout) {
        fseek(output_fd, (long)output_skip, SEEK_SET);
    }

    size_t bytes_written = 0;
    size_t bytes_read = 0;

    while (1) { 
        if (feof(input_fd)) { // ctrl-d in stdin or end of input_file
            //printf("feof catch \n");
            break;
        }

        char* buf = malloc(block_size);

        size_t read = fread(buf, 1, block_size, input_fd);
        bytes_read += read;

        size_t written = fwrite(buf, 1, read, output_fd);
        bytes_written += written;

        if (read != (size_t)block_size && input_file != NULL) { // error occurred or feof
            //printf("reached end of input file #1 \n");
            break;
        }

        if (bytes_written / block_size == total_blocks_copied) {
            //printf("read all the specified bytes #2 \n");
            break;
        }

        signal(SIGUSR1, sig_handler); // SIGUSR1 catcher that will print status report
        if (caught_signal) {
            clock_gettime(CLOCK_REALTIME, &tp);
            double end1 = (double)(tp.tv_sec + (tp.tv_nsec*(pow(10.0, -9.0)))); 

            size_t full_blocks_in = bytes_read / block_size;
            size_t partial_blocks_in = 0;
            if (bytes_read % block_size) {
                partial_blocks_in = 1;
            }

            size_t full_blocks_out = bytes_written / block_size;
            size_t partial_blocks_out = 0;
            if (bytes_written % block_size) {
                partial_blocks_out = 1;
            }
            print_status_report(full_blocks_in, partial_blocks_in,
                                full_blocks_out, partial_blocks_out,
                                bytes_written, end1 - start);
            
            caught_signal = 0;
            
        }
    }

    clock_gettime(CLOCK_REALTIME, &tp);
    double end = (double)(tp.tv_sec + (tp.tv_nsec*(pow(10.0, -9.0)))); 

    size_t full_blocks_in = bytes_read / block_size;
    size_t partial_blocks_in = 0;
    if (bytes_read % block_size) {
        partial_blocks_in = 1;
    }

    size_t full_blocks_out = bytes_written / block_size;
    size_t partial_blocks_out = 0;
    if (bytes_written % block_size) {
        partial_blocks_out = 1;
    }
    print_status_report(full_blocks_in, partial_blocks_in,
                         full_blocks_out, partial_blocks_out,
                         bytes_written, end - start);
    return 0;
}

void sig_handler(int sig) {
    caught_signal = 1;
}
