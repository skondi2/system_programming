/**
 * Password Cracker
 * CS 241 - Spring 2020
 */

//#include "cracker2.h"
#include "format.h"
#include "utils.h"
#include "crypt.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

// structs
typedef struct passwordEncryptTask {
    char* username;
    char* encrypted;
    char* encryptedHint;
} encryptTask;

typedef struct crypt_data cryptS;

typedef struct passwordEncryptThread {
    int id;
    long offset;
    long count;
    int hash_count;
    char* username;
    char* hint;
} encryptThread;

// global variables --> a barrier and condition variable
pthread_mutex_t barrier = PTHREAD_MUTEX_INITIALIZER; 
int condition_var = 0; 
encryptThread* threads;
pthread_t* actual_threads;
encryptTask* curr_task;

// helper functions
int read_stdin(); // sets the current task
void create_threads(size_t thread_count, char* hint, char* username);
void* decrypt(void* encrypted_thread);
char* find_password(char* hint_copy, encryptThread* curr_thread, cryptS cdata);

int start(size_t thread_count) {
    threads = malloc(sizeof(encryptThread) * thread_count);
    actual_threads = malloc(sizeof(pthread_t) * thread_count);
    curr_task = malloc(sizeof(encryptTask));

    while (1) {
        double start_time = getTime();
        double start_time_cpu = getCPUTime();
        
        if (read_stdin() == 0) {
            // reached the EOF, must exit loop
            break;
        }
        pthread_mutex_lock(&barrier);
        char* username = curr_task->username;
        char* hint = curr_task->encryptedHint;
        pthread_mutex_unlock(&barrier);

        v2_print_start_user(username);

        // give task to the worker threads
        create_threads(thread_count, hint, username);
        
        int total_hash_count = 0;
        char* regular_password = NULL;
        for (size_t i = 0; i < thread_count; i++) {
            void* return_val = NULL;
            pthread_join(actual_threads[i], &return_val);
            char* return_password = (char*)return_val;
            if (return_password != NULL) {
                regular_password = return_val;
            }
            total_hash_count += threads[i].hash_count;
        }

        int result = 0;
        if (regular_password == NULL) {
            result = 1;
        }
        double end_time = getTime();
        double end_time_cpu = getCPUTime();

        double time_diff = end_time - start_time;
        double cpu_diff = end_time_cpu - start_time_cpu;
        
        v2_print_summary(username, regular_password, total_hash_count,
                      time_diff, cpu_diff, result);
        
        free(regular_password);
        
        free(curr_task->username);
        free(curr_task->encrypted);
        free(curr_task->encryptedHint);

        condition_var = 0;

        //fprintf(stderr, "----------------------------------------\n");
    }

    pthread_mutex_destroy(&barrier);
    free(threads);
    free(actual_threads);

    // free the current task --> do i need to lock and unlock around here??
    //if (curr_task != NULL) {
        //free(curr_task->username);
        //free(curr_task->encrypted);
        //free(curr_task->encryptedHint);
        free(curr_task);
    //}


    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}

/**
 * Helper function to create the threads and split up the tasks
 * 
 **/
void create_threads(size_t thread_count, char* hint, char* username) {
    // compute subrange for each thread:
    int num_unknown = strlen(hint) - getPrefixLength(hint);

    // create all the worker threads:
   for (size_t i = 0; i < thread_count; i++) {
       long start = 0;
       long count = 0;

       getSubrange(num_unknown, thread_count, (int)i + 1,
                 &start, &count);
    
       threads[i].id = (int)i + 1;
       threads[i].offset = start;
       threads[i].count = count;
       threads[i].hash_count = 0;
       threads[i].username = username;
       threads[i].hint = hint;

       pthread_create(&actual_threads[i], NULL, decrypt, (void*)&threads[i]);
   }
}

/**
 * Helper function for thread to crack password with crypt_r under its assigned
 * section of the string:
 * 
 * Utilizes these format.h functions:
 * void v2_print_thread_start(int threadId, char *username, long offset,
                           char *startPassword);

 * v2_print_thread_result(int threadId, int hashCount, int result);
 * 
 **/
void* decrypt(void* encrypted_thread) {
    encryptThread* curr_thread = (encryptThread*)encrypted_thread;

    char* hint = curr_thread->hint;
    char* username = curr_thread->username;

    char* hint_copy = strdup(hint); // NEED TO FREE
    int prefix_len = getPrefixLength(hint);
    setStringPosition(hint_copy + prefix_len, curr_thread->offset);


    v2_print_thread_start(curr_thread->id, username, curr_thread->offset,
                           hint_copy);

    // crack the password!
    cryptS cdata;
    cdata.initialized = 0;

    char* output = find_password(hint_copy, curr_thread, cdata);
    return (void*)output;
}

/**
 * Helper function to decrypt the password with crypt_r
 * 
 **/
char* find_password(char* hint_copy, encryptThread* curr_thread, cryptS cdata) {

    int hash_count = 0;
    char* regular_password = NULL;

    while (1) {
        char* hashed = crypt_r(hint_copy, "xx", &cdata);
        hash_count++;

        pthread_mutex_lock(&barrier);
        if (strcmp(hashed, curr_task->encrypted) == 0) {
            // found match --> set the conditional variable
            condition_var = 1;
            regular_password = hint_copy; 
            v2_print_thread_result(curr_thread->id, hash_count, 0);
            pthread_mutex_unlock(&barrier);

            break;
        }
        pthread_mutex_unlock(&barrier);

        incrementString(hint_copy); // incrment here b/c initial hint_copy might be password

        if (hash_count == curr_thread->count) {
            // already searched all the passwords it needed to
            v2_print_thread_result(curr_thread->id, hash_count, 2);
            break;
        }

        pthread_mutex_lock(&barrier);
        if (condition_var) {
            // another thread has found it
            v2_print_thread_result(curr_thread->id, hash_count, 1);

            pthread_mutex_unlock(&barrier);
            break;
        }
        pthread_mutex_unlock(&barrier);
    }
    
    curr_thread->hash_count = hash_count;

    if (regular_password == NULL) {
        free(hint_copy);
    }
    return regular_password;
}

/**
 * Helper function to read in input and extract info
 * 
 * Returns task of the current input line
 **/
int read_stdin() {
    char* buffer = NULL;
    size_t len = 0;
    while ((getline(&buffer, &len, stdin)) != -1) {

        char* username = strtok(buffer, " ");
        char* encrypted = strtok(NULL, " ");
        char* hint = strtok(NULL, " ");

        char* username_ = strdup(username); // NEED TO FREE
        char* encrypted_ = strdup(encrypted); // NEED TO FREE
        char* hint_ = strdup(hint); // NEED TO FREE
    
        // get rid of newline char in hint
        if (hint_[strlen(hint_) - 1] == '\n') {
            hint_[strlen(hint_) - 1] = 0;
        }

        pthread_mutex_lock(&barrier);
        curr_task->username = username_;
        curr_task->encrypted = encrypted_;
        curr_task->encryptedHint = hint_;
        pthread_mutex_unlock(&barrier);

        free(buffer);
        return 1; // succeeded in initializing curr_task
    }
    free(buffer);
    return 0; // reached EOF
}
