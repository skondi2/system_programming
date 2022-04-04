/**
 * Nonstop Networking
 * CS 241 - Spring 2020
 */
#include "common.h"
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>

char **parse_args(int argc, char **argv);
verb check_args(char **args);

// From Charming Chatroom lab:
ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    //fprintf(stderr, "count is %zu \n", count);
    //fprintf(stderr, "buffer we are writing %zu \n", (size_t)*buffer);
    size_t total_sent = 0;
    ssize_t result;
    while ((total_sent < count) && (result = write(socket, buffer+total_sent, count - total_sent))) {
        //fprintf(stderr, "entering loop of write, buffer is : %s \n", buffer);
        if (result == -1 && errno == EINTR) {
            continue;
        }
        else if (result == -1) {
            return -1;
        }
        else if (result == 0) {
            return 0;
        } else {
            total_sent += result;
        }
        
    }
    return total_sent;
}

// From Charming Chatroom lab:
ssize_t read_all_from_socket(int socket, char *buffer, size_t count, int newline) {
    size_t total_read = 0;
    ssize_t result;
    while ((total_read < count) && (result = read(socket, buffer+total_read, count - total_read))) {
        if (result == 0) {
            return total_read;
        } else if (result > 0) {
            total_read += result;
        } else if (result == -1 && EINTR == errno) {
            continue;
        } else if (result == -1) {
            return -1;
        } else if (newline && buffer[0] == '\n') { // test to see if this works???
            //fprintf(stderr, "this is the buffer in read : %s \n", buffer);
            buffer[0] = 0;
            break;
        } else {
            return total_read;
        }
    }
    return total_read;
}

int get_request_success(int socket) {
    char buffer[3];
    memset(buffer, 0, 3);
    read_all_from_socket(socket, buffer, 3, true); // stops at \n
    //fprintf(stderr, "this is server response : --%s-- \n", buffer);

    //if (buffer[2] == '\n') { // its OK\n
        //buffer[2] = 0;
        if (strcmp(buffer, "OK\n") == 0) {
        //if (strstr(buffer, "OK") != NULL) {
        //if (buffer[0] == 'O' && buffer[1] == 'K') {
            return 1;
        } else {
    //} else if (buffer[5] == '\n') { // its ERROR\n
        //if (strcmp(buffer, "ERROR\n") == 0) {
        //if (strstr(buffer, "ERROR")!= NULL) {
            char buffer1[3];
            memset(buffer1, 0, 3);
            read_all_from_socket(socket, buffer1, 3, true);
            return 0;
        }
    //}
    return -1; // for debugging.. smth went wrong
}

void get_err_message(int socket) {
    char* buffer = malloc(512);
    memset(buffer, 0, 512);
    read_all_from_socket(socket, buffer, 512, true); // stops at newline
    //fprintf(stderr, "this is error buffer message : --%s-- \n", buffer);
    print_error_message(buffer);
    free(buffer);
}

void save_file_locally(int socket, char** command, size_t file_size) {
    FILE* local = fopen(command[4], "w");
    printf("OK\n");
    size_t bytes_read = 0;
    size_t left = file_size;
    size_t min = 1024;
    char buffer[1024];

    while (bytes_read < file_size) {
        memset(buffer, 0, 1024);
        if (left < 1024) {
            min = left;
        } else {
            min = 1024;
        }
        size_t read = read_all_from_socket(socket, buffer, min, 0);
        if (read == 0) {
            print_connection_closed();
            if (bytes_read < file_size) {
                print_too_little_data();
            }
            exit(1);
        }
        if (read == (size_t)-1) {
            print_invalid_response();
            exit(1);
        }
        
        fwrite(buffer, sizeof(char), read, local);
        bytes_read += read;
        left -= read;
    }

    if (read_all_from_socket(socket, buffer, 1024, false) != 0) {
        //fprintf(stderr, "entering here #2");
        print_received_too_much_data();
    }

    fclose(local);
}

void print_list(int socket, char** command, size_t list_size) {
    size_t bytes_read = 0;
    size_t left = list_size;
    size_t min = 1024;
    char buffer[1024];
    printf("OK\n");

    while (bytes_read < list_size) {
        memset(buffer, 0, 1024); 

        if (left < 1024) {
            min = left;
        } else {
            min = 1024;
        }

        //fprintf(stderr, "min is %zu \n", min);
        size_t read = read_all_from_socket(socket, buffer, min, false);
       
        if (read == 0) {
            print_connection_closed();
            if (bytes_read < list_size) {
                print_too_little_data();
            }
            exit(1);
        }
        if (read == (size_t)-1) {
            print_invalid_response();
            exit(1);
        }
        
        printf("%s", buffer);
        bytes_read += read;
        left -= read;
    }

    ssize_t leftover = read_all_from_socket(socket, buffer, 1024, false);
    if (leftover != 0) {
        print_received_too_much_data();
    }
}

void send_request(int socket, char** command) {
    // EX) VERB remotefile\n
    size_t length = strlen(command[2]) + 1 + strlen(command[3]) + 2;
    char* request = malloc(length);
    memset(request, 0, length);
    sprintf(request, "%s %s\n", command[2], command[3]);
    //fprintf(stderr, "request is --%s-- \n", request);

    ssize_t written_bytes = write_all_to_socket(socket, request, length-1);
    if (written_bytes != (ssize_t)length-1) {
        //fprintf(stderr, "Failed to send request to server \n");
        print_invalid_response();
        free(request);
        exit(1);
    }
    free(request);
}

void send_list_request(int socket, char** command) {
    // EX) LIST\n
    size_t length = strlen(command[2]) + 2;
    char* request = malloc(length);
    memset(request, 0, length);
    sprintf(request, "%s\n", command[2]);

    ssize_t written_bytes = write_all_to_socket(socket, request, length-1);
    if (written_bytes != (ssize_t)length-1) {
        //fprintf(stderr, "Failed to send list request to server \n");
        print_invalid_response();
        free(request);
        exit(1);
    }
    free(request);
}

void send_data_info(int socket, char** command) {
    FILE* local = fopen(command[4], "r");
    if (local == NULL) {
        printf("%s", err_no_such_file);
        exit(1);
    }

    // get file size and send to server:
    fseek(local, 0, SEEK_END);
    size_t size = ftell(local);
    //char* size_string = malloc(256);
    //snprintf(size_string, sizeof(size_string), "%zu", size);
    rewind(local);
    //fprintf(stderr, "this is file size : --%zu-- \n", size);

    ssize_t written_bytes = write_all_to_socket(socket, (char*)&size, sizeof(size_t));
    if (written_bytes <= 0) {
        print_invalid_response();
        exit(1);
    }
 
    // get binary data of the local file and send to server:
    char buffer[1024];
    memset(buffer, 0, 1024);
    size_t bytes_sent = 0;

    while (bytes_sent < size) {
        size_t read_bytes = fread(buffer, sizeof(char), 1024, local);
        //fprintf(stderr, "buffer written to socket is : --%s-- \n", buffer);
        size_t written_bytes1 = write_all_to_socket(socket, buffer, read_bytes);
        if (written_bytes1 == (size_t)-1) {
            print_invalid_response();
            exit(1);
        }
        if (written_bytes1 == 0) {
            print_connection_closed();
            //fprintf(stderr, "entering here \n");
            print_too_little_data();

            exit(1);
        }
        
        bytes_sent += written_bytes1;
    }

    fclose(local);
}

int main(int argc, char **argv) {
    char** command = parse_args(argc, argv);

    verb verb_request = check_args(command);

    int curr_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (curr_socket == -1) {
        perror("Failed to create socket\n");
        exit(1);
    }
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP 

    int get_addr_result = getaddrinfo(command[0], command[1], &hints, &res);
    if (get_addr_result != 0) {
        const char* message = gai_strerror(get_addr_result);
        fprintf(stderr, "%s\n", message); 
        exit(1);
    }

    int connect_res = connect(curr_socket, res->ai_addr, res->ai_addrlen);
    if (connect_res == -1) {
        perror("Failed to connect socket to port\n");
        exit(1);
    }

    // send request to server:
    if (verb_request != LIST) {
        send_request(curr_socket, command); // GET, DELETE, PUT 
    } else {
        send_list_request(curr_socket, command); // LIST
    }

    if (verb_request == PUT) { // PUT request
        send_data_info(curr_socket, command);
    }

    int shut = shutdown(curr_socket, SHUT_WR);
    if (shut != 0) {
        perror("Shutdown failed");
    }

    // read response from server:
    int success = get_request_success(curr_socket);
    //fprintf(stderr, "SUCCESS IS %d \n", success);
    if (success == 1) {
        if (verb_request == PUT || verb_request == DELETE) {
            printf("OK\n");
            print_success();
        } else if (verb_request == GET || verb_request == LIST) {

            // get file size:
            size_t file_size = 0;
            ssize_t read_bytes = read_all_from_socket(curr_socket,(char*)&file_size, sizeof(size_t), false);
            //fprintf(stderr, "file size here is %zu \n", file_size);

            if (read_bytes <= 0) {
                print_invalid_response();
                exit(1);
            }

            if (verb_request == GET) { // save into local file
                //fprintf(stderr, "entering save file locally function \n");
                save_file_locally(curr_socket, command, file_size);
            }

            if (verb_request == LIST) { // print to stdout
                print_list(curr_socket, command, file_size);
            }
        }
    } else if (success == 0) {
        get_err_message(curr_socket);
        //fprintf(stderr, "entering here #6");
        
    } /*else {
        fprintf(stderr, "SMTH WENT WRONG \n");
    }*/

    close(curr_socket);
   
    freeaddrinfo(res);
    res = NULL;

    free(command);
    return 0;
}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}
