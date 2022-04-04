/**
 * Password Cracker
 * CS 241 - Spring 2020
 */
//#include "cracker1.h"
#include "format.h"
#include "utils.h"
#include "includes/queue.h"
#include "string.h"
#include <stdio.h>
#include "crypt.h"
// yaaassss-youre a bitch vscode ----
// structs
typedef struct passwordEncryptThread {
    int id;
    int failed;
    int recovered;
    //pthread_t thread;
} encryptThread;

typedef struct passwordEncryptTask {
    char* username;
    char* encrypted;
    char* encryptedHint;
} encryptTask;

// global variables
queue* task_queue;
encryptThread* threads; 
pthread_t* actual_threads;

// helper functions
void create_threads(int thread_count);
void decrypt(void* encrypted);
void read_stdin();
int find_password(char* encrypted_password, char* encrypted_hint, int id, char* username);

int start(size_t thread_count) {
    
    // initialize the queue with unlimited capacity
    task_queue = queue_create(-1);
    threads = malloc(sizeof(encryptThread) * (int)thread_count);
    actual_threads = malloc(sizeof(pthread_t) * thread_count);

    read_stdin(); 
    
    create_threads((int)thread_count);
    
    // update the recover and failed stats from the threads
    int numRecover = 0;
    int numFail = 0;
    for (size_t i = 0; i < thread_count; i++) {
        pthread_join(actual_threads[i], NULL);
        numFail += threads[i].failed;
        numRecover += threads[i].recovered;
    }

    v1_print_summary(numRecover, numFail);

    // double free occurring here 
    /*for (size_t i = 0; i < thread_count; i++) {
        free(threads[i]);
    }*/
    free(threads);
    free(actual_threads);
    queue_destroy(task_queue);

    return 0; // DO NOT change the return code since AG uses it to check if your
              // program exited normally
}

void create_threads(int thread_count) {
    for (int i = 0; i < thread_count; i++) {

        //encryptThread* thread_enc = malloc(sizeof(encryptThread));
        threads[i].id = i+1; 
        threads[i].failed = 0;
        threads[i].recovered = 0;

        //threads[i] = thread_enc; // add it to global array

        pthread_create(&actual_threads[i], NULL, (void*)&decrypt, (void*)&threads[i]);
    }
}

void decrypt(void* encrypted_thread) {
    encryptThread* thread = (encryptThread*)encrypted_thread;
    
    // pull task from queue
    encryptTask* curr_task = queue_pull(task_queue);
    
    while (curr_task != NULL) {
        char* encryp_password = curr_task->encrypted;
        char* encryp_hint = curr_task->encryptedHint;
        char* username = curr_task->username;

        // start processing the task
        v1_print_thread_start(thread->id, username);

        // crack the password
        int result = find_password(encryp_password, encryp_hint, thread->id, username);

        if (result) { // find_password returns 1 if failed to crack code
            thread->failed++;
        } else {
            thread->recovered++;
        }
        if (curr_task != NULL) {
            free(curr_task->username);
            free(curr_task->encrypted);
            free(curr_task->encryptedHint);
            free(curr_task);
        }
        curr_task = queue_pull(task_queue); // update curr_task
    }
    queue_push(task_queue, NULL); // push it back into queue for next thread
}

/**
 * Helper function to return solved password:
 * 
 * Takes in encrypted password, password hint, and hash count.
 * Solves password using crypt_r
 * 
 * Return result of whether failed to solve or not
 **/
int find_password(char* encrypted_password, char* encrypted_hint, int id, char* username) {
    // edge case for if hint is the password:
    if (strchr(encrypted_hint, '.') == NULL) {
        v1_print_thread_result(id, username, encrypted_hint,
                            0, 0, 0);
        return 0;
    }
    
    struct crypt_data cdata;
    cdata.initialized = 0;

    int prefix_len = getPrefixLength(encrypted_hint);

    char* hint_copy = strdup(encrypted_hint); // NEED TO FREE THIS
    int length = strlen(hint_copy);
    for (int i = prefix_len; i < length; i++) {
        hint_copy[i] = 'a';
    }

    char* finished = strdup(encrypted_hint); // NEED TO FREE THIS
    for (int i = prefix_len; i < length; i++) {
        finished[i] = 'z';
    }

    int hash_count = 0;
    char* regular_password = NULL;
    
    double start_time = getThreadCPUTime();
    while (1) {
        char* hashed = crypt_r(hint_copy, "xx", &cdata);
        hash_count++;

        if (!strcmp(encrypted_password, hashed)) {
            regular_password = hint_copy;
            break;
        }
        if (!strcmp(finished, hint_copy)) {
            break;
        }
        incrementString(hint_copy);
    } 
    double end_time = getThreadCPUTime();

    int result = 0;
    if (regular_password == NULL) {
        result = 1;
    }

    // print results 
    v1_print_thread_result(id, username, regular_password,
                            hash_count, end_time - start_time, result);
    
    if (regular_password == NULL) {
        free(hint_copy);
    }
    free(regular_password);
    free(finished);

    return result;
}

/**
 * Helper function to read in input lines:
 * 
 * Stores the username, encrypted password, and password hint from the input
 * and stores it into a task and pushes into global queue.
 * 
 * Null terminates queue and each char* component of task struct
 * 
 * */
void read_stdin() {
    char* buffer = NULL;
    size_t len = 0;
    while ((getline(&buffer, &len, stdin)) != -1) {
        // create a tasks
        encryptTask* task = malloc(sizeof(encryptTask));

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

        task->username = username_;
        task->encrypted = encrypted_;
        task->encryptedHint = hint_;

        queue_push(task_queue, task);
        //free(buffer);
    }

    queue_push(task_queue, NULL); // null - terminate the queue
    
    free(buffer); 
}


