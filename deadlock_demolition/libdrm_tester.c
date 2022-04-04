/**
 * Deadlock Demolition
 * CS 241 - Spring 2020
 */
#include "libdrm.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static drm_t *drm;
static int count = 0;

void *f(void *id) {
    int *result = malloc(sizeof(int));
    drm_wait(drm, id);

    if (count == 0)
        printf("Thread %zu was first\n", *((pthread_t *)id));
    else
        printf("Thread %zu was second\n", *((pthread_t *)id));

    count++;
    *result = 5 * count;
    drm_post(drm, id);

    return result;
}

int main() {
    // Example usage of DRM
    drm = drm_init();
    
    pthread_t t1, t2;
    pthread_t t3;
    pthread_create(&t1, NULL, f, &t1);
    pthread_create(&t2, NULL, f, &t2);
    pthread_create(&t3, NULL, f, &t2);
    
    int *r1;
    int *r2;
    int *r3;
    pthread_join(t1, (void **)&r1);
    pthread_join(t2, (void **)&r2);
    pthread_join(t3, (void**)&r3);

    printf("Thread 1 with ID %zu returned %d\n", t1, *r1);
    printf("Thread 2 with ID %zu returned %d\n", t2, *r2);

    free(r1);
    free(r2);
    free(r3);
    drm_destroy(drm);

    // TODO: Add more test cases to test functionality!

    return 0;
}
