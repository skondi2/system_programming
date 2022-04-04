/**
 * Teaching Threads
 * CS 241 - Spring 2020
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "reduce.h"
#include "reducers.h"

// partners : prajnak2, abouhal2

/* You might need a struct for each task ... */
typedef struct task_ {
    int* list;
    size_t length;
    size_t startIndex;
    int result;
    reducer reduce_func;
    size_t totalLen;
} task_t;

/* You should create a start routine for your threads. */
void start_routine(void* taskParam) {
    //printf("going into start routine\n");
    task_t* task = (task_t*) taskParam;

    size_t endIndex = (task->length + task->startIndex);
    if (endIndex > task->totalLen) {
        endIndex = task->totalLen;
    }
    //printf("task->startIndex : %lu\n", (task->startIndex));
    //printf("task->endIndex : %lu\n", (endIndex));
    /*if (endIndex > task->length) {
        endIndex = task->length;
    }*/

    //printf("task->startIndex : %lu\n", (task->startIndex));
    for (size_t i = task->startIndex; i < endIndex; i++) {
        //printf("i index in list : %lu\n", i);
        //printf("element in list : %d\n", task->list[i]);
        task->result = task->reduce_func(task->result, task->list[i]);
        //printf("result saved in the task : %d\n", task->result);
    }
}

int par_reduce(int *list, size_t list_len, reducer reduce_func, int base_case,
               size_t num_threads) {
    //printf("number of threads : %zu\n", num_threads);
    if (list == NULL) return 0;

    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    task_t* tasks = malloc(sizeof(task_t) * num_threads);

    if (num_threads > list_len) {
        num_threads = list_len;
    }

    int index = 0;
    //int result = base_case;
    //size_t length = list_len / num_threads;
    size_t length = (int)(ceil((double)list_len / num_threads));
    //printf("length of sublists : %lu\n", length);
    
    for (size_t i = 0; i < num_threads; i++) {
        pthread_t tid1 = 0;

        task_t t1;
        t1.list = list;
        t1.length = length;
        t1.reduce_func = reduce_func;
        t1.result = base_case;
        t1.startIndex = index*length;
        t1.totalLen = list_len;
        //printf("here is start index for lists : %lu\n", t1.startIndex);
        
        threads[i] = tid1;
        tasks[i] = t1;
        index++;
    }

    for (size_t i = 0; i < num_threads; i++) {
        pthread_create(&(threads[i]), 0, (void*)&start_routine, (void*)(&(tasks[i])));
    }

    for (size_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    size_t totalResult = base_case;
    for (size_t y = 0; y < num_threads; y++) {
        //printf("tasks[y].result is : %d\n", tasks[y].result);
        reducer reducerY = tasks[y].reduce_func;
        totalResult = reducerY(totalResult, tasks[y].result);
    }

    free(tasks);
    free(threads);
    //printf("totalResult : %zu\n", totalResult);
    return totalResult;
}
