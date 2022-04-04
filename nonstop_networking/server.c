/**
 * Nonstop Networking
 * CS 241 - Spring 2020
 */
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include "format.h"
#include "includes/vector.h"
#include "includes/dictionary.h"
#include "common.h"
#include <unistd.h>
#include <fcntl.h>

static volatile int endSession;
static int MAX_CLIENTS = 100;
static vector* file_vec;
static dictionary* curr_connections;

typedef struct client_info {
    char* state;
    verb verb_requested;
    char header[1024];

    int off_header;
    int off_size;
    int off_ok;
    int off_err;

    size_t size;
    char* filename;
    FILE* file_ptr;

    const char* err_type;
    int command_success;
    vector* curr_list;
} client_info;

void create_curr_list(int client_fd, client_info* client);
void retrieve_file(int client_fd, client_info* client);
void delete_file(char* direc_name, int client_fd, client_info* client);

// From Charming Chatroom lab:
ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    size_t total_sent = 0;
    ssize_t result;
    while ((total_sent < count) && (result = write(socket, buffer+total_sent, count - total_sent))) {
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
ssize_t read_all_from_socket(int socket, char *buffer, size_t count, int newline, bool* finished, bool* nothing_read, int header) {
    size_t total_read = 0;
    ssize_t result;

    if (!header) {
        while ((total_read < count) && (result = read(socket, buffer+total_read, count - total_read))) {
            if (result == 0) {
                *nothing_read = true;
                return total_read;

            } else if (result == -1 && EINTR == errno) {
                continue;
            } else if (result == -1) {
                    return -1;
            } else if (result > 0) {
                total_read += result;
            }
        }
        return total_read;
    }

    if (header) {
        while (1) {
            result = read(socket, buffer, 1);
            if (result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                if (errno == EINTR) {
                    continue;
                }

                return -1;
            }
            if (result == 0) {
                return -1;
            }
            total_read += result;

            if (total_read >= count) {
                return -1;
            }

            if (buffer[0] == '\n') {
                buffer[0] = 0;
                buffer += result;
                *finished = true;
                break;
            }
            buffer += result;
        }
        return total_read;
    }
    return 0; // this prob never reaches
}


/**
 * Helper function that sets the endSession to 1 when SIGINT is received.
 * Will end server session.
 **/
void end_session(int signal) {
    endSession = 1;
}

/**
 * Helper function to create abstract socket and bind it to specified port.
 * Server will listen to maximum of 100 clients
 * Note: FROM CHARMING CHATROOM LAB
 * 
 * Returns file descriptor for server socket
 **/
int init_server(char* port) {

    int serverSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (serverSocket == -1) {
        perror("Failed to create socket \n");
        exit(1);
    }

    int optval; // set port as reuseable
    int retval = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (retval == -1) {
        perror("Setsockopt failed \n");
        exit(1);
    }

    struct addrinfo hints;
    struct addrinfo *result;
    memset(&hints, 0, sizeof(struct addrinfo)); // memset doesn't allocate mem
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int get_addr_result = getaddrinfo(NULL, port, &hints, &result);
    if (get_addr_result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_addr_result));
        exit(1);
    }

    // Bind
    if (bind(serverSocket, result->ai_addr, result->ai_addrlen) != 0) {
        perror("bind() failed \n");
        exit(1);
    }

    // Listen
    if (listen(serverSocket, MAX_CLIENTS)) {
        perror("listen() failed");
        exit(1);
    }

    freeaddrinfo(result);
    result = NULL;

    return serverSocket;
}

/**
 * 
 * 
 **/
void read_put_size(int client_fd, client_info* client, char* direc_name) {
    char* buffer = (char*)(&(client->size));
    int offset = client->off_size;
    buffer += offset;

    bool f = false;
    int bytes_read = read_all_from_socket(client_fd, buffer, sizeof(size_t) - offset, false, &f, &f, false);
   
    if (bytes_read == -1) {
        // error in parsing
        client->state = "Writing status";
        client->command_success = -1; // there was error
        client->err_type = err_bad_file_size;

        return;
    }

    if (bytes_read != (sizeof(size_t)- offset)) {
        client->off_size += bytes_read;
    } else {
        client->state = "Reading PUT file";
        /*char path[strlen(direc_name) + 2 + strlen(client->filename)];
        sprintf(path, "%s/%s", direc_name, client->filename);

        client->file_ptr = fopen(path, "w");*/
    }

}

/**
 * 
 * 
 **/
void read_put_file(int client_fd, client_info* client, char* direc_name) {
    // need to put file in temp directory
    char path[strlen(direc_name) + 2 + strlen(client->filename)];
    sprintf(path, "%s/%s", direc_name, client->filename);
    char filename_cpy[1024];

    memcpy(filename_cpy, client->filename, strlen(client->filename)+1);
    //fprintf(stderr, "read_put_file(): client->filename = %s \n", filename_cpy);

    FILE* file_ptr = fopen(path, "w");
    if (file_ptr == NULL) {
        // error
        //fprintf(stderr, "error in opening the PUT file\n");
        client->state = "Writing status";
        client->command_success = -1; // there was error
        client->err_type = err_no_such_file;
        return;
    } else {
        client->file_ptr = file_ptr;
    }

    // read contents of file into buffer
    char buffer[1024];
    size_t cur_pos = ftell(client->file_ptr);

    while (cur_pos < client->size) {
        
        memset(buffer, 0, 1024);

        int count = client->size - cur_pos;
        if (count > 1024) {
            count = 1024;
        }

        bool nothing_read = false;
        bool f = false;
        int bytes_read = read_all_from_socket(client_fd, buffer, count, false, &f, &nothing_read, false);
        if (bytes_read == -1 || nothing_read == true) {
            if (nothing_read) {
                //fprintf(stderr, "read_put_file(): nothing read\n");
            }
            // remove from temp directory
            unlink(path);

            fclose(client->file_ptr);

            client->state = "Writing status";
            client->command_success = -1; // there was error
            client->err_type = err_bad_file_size;
            //fprintf(stderr, "read_put_file() : error in reading from socket \n");
            return;
        }

        fwrite(buffer, 1, bytes_read, client->file_ptr);
        if (bytes_read != count) break;

        cur_pos = ftell(client->file_ptr); // update file position
    }
    // read all the bytes of the file
    if (cur_pos == client->size) {
        bool finished = false;
        bool nothing_read = false;
        // received extra file contents
        if (read_all_from_socket(client_fd, buffer, 1024, false, &finished, &nothing_read, false)) {
            //remove file from directory:
            unlink(path);

            client->state = "Writing status";
            client->command_success = -1; // there was error
            client->err_type = err_bad_file_size;
            //fprintf(stderr, "read_put_file(): received extra file contents. file size was %zu\n", client->size);
            fclose(client->file_ptr);

            client->filename = filename_cpy;
            //fprintf(stderr, "read_put_file() : before returning, filename = %s\n", client->filename);
            return;
        }

        // add file to vector
        vector_push_back(file_vec, filename_cpy);
        //fprintf(stderr, "read_put_file() : adding filename to vector: %s\n", filename_cpy);

        // next state is write status
        client->state = "Writing status";
        client->command_success = 1;
        client->err_type = NULL;

        fclose(client->file_ptr);
        client->filename = filename_cpy;
        //fprintf(stderr, "read_put_file() : before returning, filename = %s\n", client->filename);
    }
}

/**
 * Helper function to write OK or ERROR indicating whether request 
 * was executed properly or not
 * 
 * Return 1 if server doesn't need to do anything more
 * Return 0 otherwise if execution must continue
 **/
int write_status(int client_fd, client_info* client) {
    //fprintf(stderr, "write_status() entering\n");
    
    if (client->command_success != -1) { // write OK
        //intf(stderr, "write_status(): writing OK\n");

        char* ok = "OK\n";
        int offset = client->off_ok;
        ok += offset;

        int bytes_written = write_all_to_socket(client_fd, ok, 3 - offset);
        if (bytes_written == -1) {
            // failed to write message
            // DONE:
            return 1;
        }

        if (bytes_written == 3 - offset) { // wrote out the message
            if (client->verb_requested == GET || client->verb_requested == LIST) {
                // set client to next state
                client->state = "Writing GET/LIST size";
                return 0;
            }
            // DONE: 
            return 1;
        } else {
            client->off_ok += bytes_written;
        }
    } 
    
    if (client->command_success == -1) { // write ERROR 
        //fprintf(stderr, "write_status: writing ERROR \n");

        char* error = "ERROR\n";
        const char* err_message = client->err_type;

        char *result = malloc(strlen(error) + strlen(err_message) + 1); // +1 for the null-terminator
      
        strcpy(result, error);
        strcat(result, err_message);
        //fprintf(stderr, "write_status(): final error message is --%s-- \n", result);

        int offset = client->off_err;
        result += offset;

        int bytes_written1 = write_all_to_socket(client_fd, result, strlen(result));
        if (bytes_written1 == -1 || bytes_written1 == (int)strlen(result)) {
            // DONE:
            if (bytes_written1 == -1) {
                //fprintf(stderr, "write_status(): write() returned -1 \n");
            }
            else {
                //fprintf(stderr, "write_status(): write() wrote everything \n");
            }
            return 1;
        }

        // not done writing message yet
        client->off_err += bytes_written1;
    }
    return 0;
}


/**
 * Helper function to process the first state of a client request:
 * 
 * Reads the header into a buffer and parses the buffer into requested verb
 * and filename. Then calls helper function to execute verb request
 * 
 * Sets state to error when parsing or reading fails
 * 
 **/
void read_header(int client_fd, client_info* client, char* direc_name) {
    //fprintf(stderr, "start of read_header()");

    char* buffer = client->header;
    int offset = client->off_header;
    buffer += offset;
    
    bool f = false;
    bool status = false;
    int bytes_read = read_all_from_socket(client_fd, buffer, 1024 - offset, false, &status, &f, 1);
    
    if (bytes_read == -1) {
        // error parsing --> change state to WRITING STATUS:
        //fprintf(stderr, "read_header() : moving to error state\n");
        client->state = "Writing status";
        client->command_success = -1; // there was error
        client->err_type = err_bad_request;
        return;
    }

    // check whether finished parsing header or not
    if (status == true) {
        //fprintf(stderr, "read_header(): finished reading, buffer is --%s--\n", buffer);

        char verb_requested[7];
        char filename[1024];

        int var_filled = sscanf(client->header, "%s %s", verb_requested, filename);
        
        if ((var_filled != 2 && strcmp(verb_requested, "LIST")) || (strcmp(verb_requested, "LIST") == 0 && var_filled != 1)) {
            // error parsing --> change state to WRITING STATUS:
            //fprintf(stderr, "read_header() : sscanf of header failed: %s\n", client->header);
            client->state = "Writing status";
            client->command_success = -1; // there was error
            client->err_type = err_bad_request;
            return;
        }
        client->filename = filename;
        //intf(stderr, "read_header(): setting client->filename = %s \n", client->filename);

        if (strcmp(verb_requested, "LIST") == 0) {
            client->verb_requested = LIST;
            create_curr_list(client_fd, client);

            // change status to write status
            client->state = "Writing status";
        }
        else if (strcmp(verb_requested, "GET") == 0) {
            client->verb_requested = GET;
            retrieve_file(client_fd, client);

            // change status to write status
            client->state = "Writing status";
        }
        else if (strcmp(verb_requested, "PUT") == 0) {
            client->verb_requested = PUT;

            // change status to reading put size:
            client->state = "Reading PUT size";
        }
        else if (strcmp(verb_requested, "DELETE") == 0) {
            client->verb_requested = DELETE;
            delete_file(direc_name, client_fd, client);

            // change status to write status
            client->state = "Writing status";
        } else {
            // verb requested --> change state to WRITING STATUS:
            //fprintf(stderr, "incorrect verb was requested\n");
            client->state = "Writing status";
            client->command_success = -1; // there was error
            client->err_type = err_bad_request;
            return;
        }
    } else if (status == false) {
        client->off_header += bytes_read;
        //fprintf(stderr, "read_header() : incrementing the offset");
        return;
    }
}

/**
 * Helper function to clean up epoll and dictionary when a client
 * request has finished being executed by server
 **/
void cleanup(int epollfd, int client_fd) {
    dictionary_remove(curr_connections, &client_fd); // remove from dictionary

    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_fd, NULL); // remove from epoll instance

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
}

/**
 * 
 * 
 **/
void write_size(int client_fd, client_info* client, int epollfd) {
    //fprintf(stderr, "entering write size \n");

    char* buffer = (char*)&client->size;
    //fprintf(stderr, "write_size() : file size is %zu \n", client->size);
    int offset = client->off_size;
    buffer += offset;

    int bytes_written = write_all_to_socket(client_fd, buffer, sizeof(size_t) - offset);
    if (bytes_written == -1) {
        // failed to write
        //fprintf(stderr, "yay its cleaning up \n");
        cleanup(epollfd, client_fd);
    }
    if (bytes_written == sizeof(size_t) - offset) {
        if (client->verb_requested == LIST) {
            // transition to write list state
            //fprintf(stderr, "gonna go write the list \n");
            client->state = "Writing list";
        }
        if (client->verb_requested == GET) {
            // transition to retrieve file state
            client->state = "Writing file";
        }
    } else {
        // not finished wriitng yet
        client->off_size += bytes_written;
    }
}

/**
 * 
 **/
void print_file(int client_fd, client_info* client, int epollfd) {
    char* buffer = malloc(1024);

    size_t cur_pos = 0;

    while (cur_pos < client->size) {
        cur_pos = ftell(client->file_ptr);

        int count = client->size - cur_pos;
        if (count > 1024) {
            count = 1024;
        }

        fread(buffer, 1, count, client->file_ptr);

        int bytes_written = write_all_to_socket(client_fd, buffer, count);
        if (bytes_written == -1) {
            cleanup(epollfd, client_fd);
            fclose(client->file_ptr);
            free(buffer);
            return;
        }

        if (bytes_written < count) { // didn't write all the desired bytes
            fseek(client->file_ptr, bytes_written - count, SEEK_CUR);
            break;
        }
    }

    if (cur_pos == client->size) {
        fclose(client->file_ptr);
        cleanup(epollfd, client_fd);
    }
    free(buffer);
}

/**
 * 
 **/
void print_list(int client_fd, client_info* client, int epollfd) {
    //fprintf(stderr, "entering print_list()\n");

    for (size_t i = 0; i < vector_size(client->curr_list); i++) {
        char* filename = (char*)vector_get(client->curr_list, i);
        size_t count = strlen(filename);

        //fprintf(stderr, "print_list: writing this filename: %s \n", filename);
        int bytes_written = write_all_to_socket(client_fd, filename, count);
        if (bytes_written == -1) {
            // failed to write
            cleanup(epollfd, client_fd);
        }
        if (i != vector_size(client->curr_list) - 1) {
            int byte = write_all_to_socket(client_fd, "\n", 1);
            if (byte == -1) {
                cleanup(epollfd, client_fd);
            }
        }
    }
    //fprintf(stderr, "print_list(): going to cleanup \n");
    cleanup(epollfd, client_fd);
}

/**
 * 
 **/
void create_curr_list(int client_fd, client_info* client) {
    //fprintf(stderr, "entering create_curr_list() \n");
    vector* curr_list = string_vector_create();
    size_t list_len = 0;

    for (size_t i = 0; i < vector_size(file_vec); i++) {
        //fprintf(stderr, "create_curr_list(): pushing back into list: --%s-- \n", (char*)vector_get(file_vec, i));
        list_len += strlen((char*)vector_get(file_vec, i));
        vector_push_back(curr_list, (char*)vector_get(file_vec, i));
    }
    
    client->curr_list = curr_list;
    //fprintf(stderr, "create_curr_list() size %zu \n", vector_size(client->curr_list));
    client->size = list_len;
}

/**
 * 
 **/
void retrieve_file(int client_fd, client_info* client) {
    // check if file is in file vector
    for (size_t i = 0; i < vector_size(file_vec); i++) {
        char* curr_file = (char*)vector_get(file_vec, i);
        if (strcmp(curr_file, client->filename) == 0) {
            // then open the file and do ftell to get the size
            FILE* file_ptr = fopen(curr_file, "r");
            if (file_ptr == NULL) {
                client->state = "Writing status";
                client->command_success = -1; // there was error
                client->err_type = err_no_such_file;
                return;
            }
            client->file_ptr = file_ptr;

            fseek(client->file_ptr, 0, SEEK_END);
            client->size = ftell(client->file_ptr);
            rewind(client->file_ptr);

            return;
        }
    }

    // file isn't in vector
    client->state = "Writing status";
    client->command_success = -1; // there was error
    client->err_type = err_no_such_file;
    return;
    
}


/**
 * 
 **/
void delete_file(char* direc_name, int client_fd, client_info* client) {
    // check if file is in file vector
    //fprintf(stderr, "entering delete_file()\n");

    vector* new_file_vec = string_vector_create();

    //size_t erase_index = -1;
    for (size_t i = 0; i < vector_size(file_vec); i++) {
        char* curr_file = (char*)vector_get(file_vec, i);
        if (strcmp(curr_file, client->filename) == 0) {
            // then delete from directory:
            char path[strlen(direc_name) + 2 + strlen(client->filename)];
            sprintf(path, "%s/%s", direc_name, client->filename);
            unlink(path);

            //fprintf(stderr, "delete_file(): erasing from file vec %s \n", client->filename);
            //erase_index = i;
            //break;
        } else {
            vector_push_back(new_file_vec, curr_file);
        }
    }

    file_vec = new_file_vec;

    /*if (erase_index != (size_t)-1) { // erase from vector
        fprintf(stderr, "trying to erase this index %zu \n", erase_index);
        fprintf(stderr, "trying to erase this elem: %s \n", vector_get(file_vec, erase_index));
        fprintf(stderr, "vector size is %zu \n", vector_size(file_vec));
        vector_erase(file_vec, erase_index);
    }*/

    //vector* new_curr_list = string_vector_create();
    size_t erase_index1 = -1;
    //fprintf(stderr, "before \n");
    //fprintf(stderr, "SIZE OF client->curr_list %zu \n", vector_size(client->curr_list));
    //fprintf(stderr, "after \n");

    for (size_t i = 0; i < vector_size(client->curr_list); i++) {
        char* curr_file = (char*)vector_get(client->curr_list, i);
        if (strcmp(curr_file, client->filename) == 0) {
            // then delete from directory:
            
            //fprintf(stderr, "delete_file(): erasing from client->curr_list %s \n", client->filename);
            erase_index1 = i;
            break;
        }
    }
    if (erase_index1 != (size_t)-1) { // erase from vector
        //fprintf(stderr, "trying to erase1 this index %zu \n", erase_index1);
        vector_erase(client->curr_list, erase_index1);
    }

    // file isn't in vector
    client->state = "Writing status";
    client->command_success = -1; // there was error
    client->err_type = err_no_such_file;
    return;
}

/**
 * Helper function to initialize the global file vector, global commands directory,
 * and create the temporary directory to store files PUT to the server
 * 
 * Returns name of temporary directory
 **/
char* init_data_structures() {
    file_vec = string_vector_create();

    curr_connections = int_to_shallow_dictionary_create();

    char template[] = "XXXXXX";
    char* direc_name = mkdtemp(template);
    print_temp_directory(direc_name);
    return strdup(direc_name);
}

/**
 * 
 **/
int main(int argc, char **argv) {
    if (argc != 2) {
        print_server_usage();
        exit(1);
    }
    
    endSession = 0;
    char* port = argv[1];

    int serverSocket = init_server(port); // initialize server

    char* direc_name = init_data_structures(); // initialize temp directory, vector, dict

    int epollfd = epoll_create1(0); // initialize epoll 
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(1);
    }
    struct epoll_event event;
    struct epoll_event event_array[MAX_CLIENTS + 1]; // +1 for the serverSocket

    event.events = EPOLLIN; // for reading
    event.data.fd = serverSocket;
    int epoll_add = epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSocket, &event);
    if (epoll_add == -1) {
        perror("failed to add to epoll instance");
        exit(1);
    }

    struct sigaction* sa = malloc(sizeof(struct sigaction)); // initiaize sigaction variables
    sa->sa_handler = end_session;
    sigemptyset(&sa->sa_mask);
    sa->sa_flags = 0;
    if (sigaction(SIGINT, sa, NULL) == -1) {
        perror("failed to create signal catcher");
    }

    int ready_clients = 0;
    client_info client;

    while (endSession != 1) {

        ready_clients = epoll_wait(epollfd, event_array, MAX_CLIENTS+1, -1);
        if (ready_clients == -1) {
            perror("epoll wait failed");
            exit(1);
        }

        for (int i = 0; i < ready_clients; i++) {
            
            int fd = event_array[i].data.fd;
            if (fd != serverSocket) { // deal with exisitng client
                if (dictionary_contains(curr_connections, &fd) == false) {
                    perror("connection not in dictionary yet");
                    exit(1);
                }
                int client_fd = fd;
                client_info* client = dictionary_get(curr_connections, &fd);

                // check the state the client
                if (strcmp(client->state, "Reading header") == 0) {
                    read_header(client_fd, client, direc_name);
                    //fprintf(stderr, "client status is now --%s--\n", client->state);

                } else if (strcmp(client->state, "Reading PUT size") == 0) {
                    read_put_size(client_fd, client, direc_name);
                    client->off_size = 0;// clearing it here?

                } else if (strcmp(client->state, "Reading PUT file") == 0) {
                    read_put_file(client_fd, client, direc_name);
                    //fprintf(stderr, "exiting read_put_file() \n");

                } else if (strcmp(client->state, "Writing status") == 0) {
                    // change epoll to write:
                    struct epoll_event ev1;
                    ev1.events = EPOLLOUT; // for writing
                    ev1.data.fd = client_fd;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, client_fd, &ev1);

                    int output = write_status(client_fd, client);
                    if (output == 1) {
                        cleanup(epollfd, client_fd);
                        //fprintf(stderr, " main(), just finished cleanup after write_status\n");
                    }
                    
                } else if (strcmp(client->state, "Writing GET/LIST size") == 0) {
                    write_size(client_fd, client, epollfd);

                } else if (strcmp(client->state, "Writing file") == 0) {
                    print_file(client_fd, client, epollfd);

                } else if (strcmp(client->state, "Writing list") == 0) {
                    print_list(client_fd, client, epollfd);

                }
            } 
            
            if (fd == serverSocket) { // accept new client
                int client_fd = accept(serverSocket, NULL, NULL);
                if (client_fd == -1) {
                    perror("accept() failed");
                    exit(1);
                }

                // set client fd as non-blocking
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                event.events = EPOLLIN; // add to epoll instance
                event.data.fd = client_fd;
                int epoll_add1 = epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event);
                if (epoll_add1 == -1) {
                    perror("failed to add to epoll instance1");
                    exit(1);
                }

                // add to dictionary
                memset(&client, 0, sizeof(client_info));
                client.state = "Reading header";
                
                client.off_header = 0;
                client.off_err = 0;
                client.off_ok = 0;
                client.off_size = 0;
                dictionary_set(curr_connections, &client_fd, &client); 
            }
        }
    }

    close(epollfd);
}
