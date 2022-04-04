/**
 * Charming Chatroom
 * CS 241 - Spring 2020
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include "utils.h"

#define MAX_CLIENTS 8
// partner: nehap2, skondi2
void *process_client(void *p);
void cleanup();

static volatile int serverSocket;
static volatile int endSession;

static volatile int clientsCount;
static volatile int clients[MAX_CLIENTS];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Signal handler for SIGINT.
 * Used to set flag to end server.
 */
void close_server() {
    endSession = 1;
    // add any additional flags here you want.
    clientsCount = 0;
    
    //cleanup();

    //pthread_mutex_destroy(&mutex);
}

/**
 * Cleanup function called in main after `run_server` exits.
 * Server ending clean up (such as shutting down clients) should be handled
 * here.
 */
void cleanup() {
    if (shutdown(serverSocket, SHUT_RDWR) != 0) {
        perror("shutdown():");
    }
    close(serverSocket);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            if (shutdown(clients[i], SHUT_RDWR) != 0) {
                perror("shutdown(): ");
            }
            close(clients[i]);
        }
    }
}

/**
 * Sets up a server connection.
 * Does not accept more than MAX_CLIENTS connections. If more than MAX_CLIENTS
 * clients attempts to connects, simply shuts down
 * the new client and continues accepting.
 * Per client, a thread should be created and 'process_client' should handle
 * that client.
 * Makes use of 'endSession', 'clientsCount', 'client', and 'mutex'.
 *
 * port - port server will run on.
 *
 * If any networking call fails, the appropriate error is printed and the
 * function calls exit(1):
 *    - fprtinf to stderr for getaddrinfo
 *    - perror() for any other call
 */

// sources: CS 241 Coursebook Ch 11 and Lectures 25-26

void run_server(char *port) {
    endSession = 0;
    /*QUESTION 1*/
    /*QUESTION 2*/
    /*QUESTION 3*/

    /*QUESTION 8*/

    /*QUESTION 4*/
    /*QUESTION 5*/
    /*QUESTION 6*/

    /*QUESTION 9*/

    /*QUESTION 10*/

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("failed to create socket \n");
        exit(1);
    }
    // set socket ports as reusable
    int optval = 1;
    int retval = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (retval == -1) {
        perror("setsockopt failed \n");
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

    // Bind and listen
    if (bind(serverSocket, result->ai_addr, result->ai_addrlen) != 0) {
        perror("bind() failed \n");
        exit(1);
    }

    if (listen(serverSocket, MAX_CLIENTS) != 0) {
        perror("listen() failed \n");
        exit(1);
    }

    // ready to listen for clients:
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1; // following pattern in cleanup()
    }

    while (!endSession) {

        if (clientsCount < MAX_CLIENTS) {
           
            printf("Waiting for clients .. \n");
            struct sockaddr clientaddr;
            socklen_t clientaddrsize = sizeof(struct sockaddr);
            memset(&clientaddr, 0, sizeof(struct sockaddr));

            int client_fd = accept(serverSocket, &clientaddr, &clientaddrsize);
            if (client_fd == -1) { // NOW IT ENTERS THIS IF-STATEMENT
                perror("accepting client failed \n");
                exit(1);
            }

            intptr_t cid = 0;
            pthread_mutex_lock(&mutex); // LOCK

            // add client_fd to empty spot in clients array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == -1) {
                    clients[i] = client_fd;
                    cid = i; // found index
                    break;
                 }
            }
            clientsCount++;
            pthread_mutex_unlock(&mutex); // UNLOCK

            pthread_t thread = 0;
            int create_thread = pthread_create(&thread, NULL, process_client, (void*)cid);
            
            if (create_thread != 0) {
                perror("pthread create failed \n");
                exit(1);
            }
        }

        //printf("Waiting for connection ... \n");
        
    }
    //if more than MAX_CLIENTS clients attempts to connects, simply shuts down
    // the new client and continues accepting.
    freeaddrinfo(result);
    result = NULL;
}

/**
 * Broadcasts the message to all connected clients.
 *
 * message  - the message to send to all clients.
 * size     - length in bytes of message to send.
 */

void write_to_clients(const char *message, size_t size) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            ssize_t retval = write_message_size(size, clients[i]);
            if (retval > 0) {
                retval = write_all_to_socket(clients[i], message, size);
            }
            if (retval == -1) {
                perror("write(): ");
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Handles the reading to and writing from clients.
 *
 * p  - (void*)intptr_t index where clients[(intptr_t)p] is the file descriptor
 * for this client.
 *
 * Return value not used.
 */
void *process_client(void *p) {
    pthread_detach(pthread_self());
    intptr_t clientId = (intptr_t)p;
    ssize_t retval = 1;
    char *buffer = NULL;

    while (retval > 0 && endSession == 0) {
        retval = get_message_size(clients[clientId]);
        if (retval > 0) {
            buffer = calloc(1, retval);
            retval = read_all_from_socket(clients[clientId], buffer, retval);
        }
        if (retval > 0)
            write_to_clients(buffer, retval);

        free(buffer);
        buffer = NULL;
    }

    printf("User %d left\n", (int)clientId);
    close(clients[clientId]);

    pthread_mutex_lock(&mutex);
    clients[clientId] = -1;
    clientsCount--;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "%s <port>\n", argv[0]);
        return -1;
    }

    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = close_server;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    run_server(argv[1]);
    cleanup();
    pthread_exit(NULL);
}