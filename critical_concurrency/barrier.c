/**
 * Critical Concurrency
 * CS 241 - Spring 2020
 */
#include "barrier.h"
// partner is nehap2 and prajnak2

// The returns are just for errors if you want to check for them.
int barrier_destroy(barrier_t *barrier) {
    pthread_cond_destroy(&(barrier->cv));
    pthread_mutex_destroy(&(barrier->mtx));
    return 0;
}

int barrier_init(barrier_t *barrier, unsigned int num_threads) {
    int mutex_error = pthread_mutex_init(&(barrier->mtx), NULL);
    int cond_error = pthread_cond_init(&(barrier->cv), NULL);
    barrier->n_threads = num_threads;
    barrier->count = 0;
    barrier->times_used = 1;

    int error = mutex_error | cond_error;

    return error;
}

int barrier_wait(barrier_t *barrier) {
    int error = 0;
    error = pthread_mutex_lock(&barrier->mtx);

    while (barrier->times_used != 1) {
        pthread_cond_wait(&barrier->cv, &barrier->mtx);
    }
    if (barrier->count == 0) {
        barrier->count = barrier->n_threads;
    }
    barrier->count--;

    if (barrier->count == 0) {
        barrier->count++;
        barrier->times_used = 0;
        error = pthread_cond_broadcast(&barrier->cv);
    } else {
        while (barrier->count > 0 && barrier->times_used == 1) {
            pthread_cond_wait(&barrier->cv, &barrier->mtx);
        }

        barrier->count ++;

        if (barrier->count == barrier->n_threads) {
            barrier->times_used = 1;

        }
         error = pthread_cond_broadcast(&barrier->cv);
    }
   error= pthread_mutex_unlock(&barrier->mtx);
   return error;
}

