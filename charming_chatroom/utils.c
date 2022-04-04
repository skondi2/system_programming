/**
 * Charming Chatroom
 * CS 241 - Spring 2020
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// partner: nehap2, skondi2
#include "utils.h"
static const size_t MESSAGE_SIZE_DIGITS = 4;

char *create_message(char *name, char *message) {
    int name_len = strlen(name);
    int msg_len = strlen(message);
    char *msg = calloc(1, msg_len + name_len + 4);
    sprintf(msg, "%s: %s", name, message);

    return msg;
}

ssize_t get_message_size(int socket) {
    int32_t size;
    ssize_t read_bytes =
        read_all_from_socket(socket, (char *)&size, MESSAGE_SIZE_DIGITS);
    if (read_bytes == 0 || read_bytes == -1)
        return read_bytes;

    return (ssize_t)ntohl(size);
}

// You may assume size won't be larger than a 4 byte integer
ssize_t write_message_size(size_t size, int socket) {
    // Your code here
    int32_t size_ = htonl(size);
    ssize_t write_bytes =
        write_all_to_socket(socket, (char *)&size_, MESSAGE_SIZE_DIGITS);
    
    // if (write_bytes == 0 || write_bytes == -1) {
    //     return write_bytes;
    // }
    return write_bytes;
}

// source: CS 241 Textbook and lectures 25 & 26
ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
    // Your Code Here
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
        } else {
            return total_read;
        }
    }
    return total_read;
}

// source: CS 241 Textbook and lectures 25 & 26
ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    // Your Code Here
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