/**
 * Savvy Scheduler
 * CS 241 - Spring 2020
 */
#include "libpriqueue/libpriqueue.h"
#include "libscheduler.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "print_functions.h"

/**
 * The struct to hold the information about a given job
 */
typedef struct _job_info {
    int id;

    int arrival_time;
    int start_time;
    int end_time;
    int run_time;
    int ready_q_time; // time since been on the queue
    int remaining_time;

    int priority_val;
} job_info;

typedef struct _scheduler_info {
    int response_time;
    int waiting_time;
    int turnaround_time;
} sched_info;

static int num_jobs;
static sched_info* scheduler;
//static priqueue_t pqueue;
//static scheme_t pqueue_scheme;

void scheduler_start_up(scheme_t s) {
    switch (s) {
    case FCFS:
        comparision_func = comparer_fcfs;
        break;
    case PRI:
        comparision_func = comparer_pri;
        break;
    case PPRI:
        comparision_func = comparer_ppri;
        break;
    case PSRTF:
        comparision_func = comparer_psrtf;
        break;
    case RR:
        comparision_func = comparer_rr;
        break;
    case SJF:
        comparision_func = comparer_sjf;
        break;
    default:
        printf("Did not recognize scheme\n");
        exit(1);
    }
    priqueue_init(&pqueue, comparision_func);
    pqueue_scheme = s;
    // Put any additional set up code you may need here
    num_jobs = 0;

    scheduler = malloc(sizeof(sched_info));
    scheduler->response_time = 0;
    scheduler->turnaround_time = 0;
    scheduler->waiting_time = 0;

}

static int break_tie(const void *a, const void *b) {
    return comparer_fcfs(a, b);
}

int comparer_fcfs(const void *a, const void *b) {
    job* a_job = (job*)a;
    job* b_job = (job*)b;

    job_info* a_info = a_job->metadata;
    job_info* b_info = b_job->metadata;

    if (a_info->arrival_time < b_info->arrival_time) {
        return -1; // a is first
    }
    if (a_info->arrival_time == b_info->arrival_time) {
        return 0; // arrived at same time
    }
    return 1; // b is first
}

int comparer_ppri(const void *a, const void *b) {
    return comparer_pri(a, b);
}

int comparer_pri(const void *a, const void *b) {
    job* a_job = (job*)a;
    job* b_job = (job*)b;

    job_info* a_info = a_job->metadata;
    job_info* b_info = b_job->metadata;

    if (a_info->priority_val > b_info->priority_val) {
        return 1; // b lower priority
    } 
    if (a_info->priority_val == b_info->priority_val) {
        return break_tie(a, b); // same priority
    }
    return -1; // a lower priority
}

int comparer_psrtf(const void *a, const void *b) {
    job* a_job = (job*)a;
    job* b_job = (job*)b;

    job_info* a_info = a_job->metadata;
    job_info* b_info = b_job->metadata;

    if (a_info->remaining_time > b_info->remaining_time) {
        return 1; // b has less remaining
    } 
    if (a_info->remaining_time == b_info->remaining_time) {
        return break_tie(a, b); // same remaining time
    }
    return -1; // a has less remaining time
}

int comparer_rr(const void *a, const void *b) {
    job* a_job = (job*)a;
    job* b_job = (job*)b;

    job_info* a_info = a_job->metadata;
    job_info* b_info = b_job->metadata;

    if (a_info->ready_q_time > b_info->ready_q_time) {
        return 1; // a has been on ready queue longer
    } 
    if (a_info->ready_q_time == b_info->ready_q_time) {
        return break_tie(a, b); // same ready queue time
    }
    return -1; // b has been on ready queue longer
}

int comparer_sjf(const void *a, const void *b) {
    job* a_job = (job*)a;
    job* b_job = (job*)b;

    job_info* a_info = a_job->metadata;
    job_info* b_info = b_job->metadata;

    if (a_info->run_time > b_info->run_time) {
        return 1; // b less runtime
    } 
    if (a_info->run_time == b_info->run_time) {
        return break_tie(a, b); // same runtime
    }
    return -1; // a less runtime
}

// Do not allocate stack space or initialize ctx. These will be overwritten by
// gtgo
void scheduler_new_job(job *newjob, int job_number, double time,
                       scheduler_info *sched_data) {
    num_jobs++;

    job_info* newjob_info = malloc(sizeof(job_info));
    newjob_info->id = job_number;
    newjob_info->arrival_time = time;
    newjob_info->end_time = -1;
    newjob_info->start_time = -1;
    newjob_info->remaining_time = sched_data->running_time;
    newjob_info->run_time = sched_data->running_time;
    newjob_info->ready_q_time = time;
    newjob_info->priority_val = sched_data->priority;

    newjob->metadata = newjob_info;

    priqueue_offer(&pqueue, newjob); // add to queue

}

job *scheduler_quantum_expired(job *job_evicted, double time) {
    job* possible_next = (job*)priqueue_peek(&pqueue);
    if (possible_next == NULL) return NULL;

    if (job_evicted == NULL) {
        return possible_next;
    }

    job_info* evicted_info = (job_info*)job_evicted->metadata;
    evicted_info->ready_q_time = time;
    evicted_info->remaining_time -= time;
    if (pqueue_scheme != PPRI && pqueue_scheme != PSRTF && pqueue_scheme != RR) {
        return job_evicted;
    }
    priqueue_offer(&pqueue, job_evicted);
    
    job* next_job = (job*)priqueue_poll(&pqueue);
    job_info* next_info = (job_info*)next_job->metadata;

    next_info->ready_q_time = time;
    scheduler->response_time += (time - next_info->arrival_time);

    return next_job;
}

void scheduler_job_finished(job *job_done, double time) {
    job_info* done_info = (job_info*)job_done->metadata;

    done_info->end_time = time;
    done_info->remaining_time = 0;
    done_info->ready_q_time = 0; 

    scheduler->waiting_time += ((done_info->end_time - done_info->arrival_time) - 
        done_info->run_time);
    scheduler->turnaround_time += (done_info->end_time - done_info->arrival_time);
}

static void print_stats() {
    fprintf(stderr, "turnaround     %f\n", scheduler_average_turnaround_time());
    fprintf(stderr, "total_waiting  %f\n", scheduler_average_waiting_time());
    fprintf(stderr, "total_response %f\n", scheduler_average_response_time());
}

double scheduler_average_waiting_time() {
    return (double)(scheduler->waiting_time / num_jobs);
}

double scheduler_average_turnaround_time() {
    return (double)(scheduler->turnaround_time / num_jobs);
}

double scheduler_average_response_time() {
    return (double)(scheduler->response_time / num_jobs);
}

void scheduler_show_queue() {
    // OPTIONAL: Implement this if you need it!
}

void scheduler_clean_up() {
    priqueue_destroy(&pqueue);
    print_stats();
}
