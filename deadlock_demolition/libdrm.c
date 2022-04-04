/**
 * Deadlock Demolition
 * CS 241 - Spring 2020
 */
#include "graph.h"
#include "libdrm.h"
#include "set.h"
#include <pthread.h>

// helper functions:
int dfs(void* ragNode);

// You probably will need some global variables here to keep track of the
// resource allocation graph.
static graph* rag = NULL; // vertices are pthread_t* and drm_t*
static set* nodes = NULL; // set to keep track of visited nodes in graph
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER; // used to make graph thread safe
static int resetSet = 0;


struct drm_t {
    // Declare your struct's variables here. Think about what you will need.
    // Hint: You will need at least a synchronization primitive.
    pthread_mutex_t mutex;
};

drm_t *drm_init() {
    drm_t* drm = malloc(sizeof(drm_t));
    pthread_mutex_init(&(drm->mutex), NULL);

    pthread_mutex_lock(&global_mutex);
    if (rag == NULL) {
        rag = shallow_graph_create();
    }
    graph_add_vertex(rag, drm);
    pthread_mutex_unlock(&global_mutex);

    return drm;
}

int drm_post(drm_t *drm, pthread_t *thread_id) {
    pthread_mutex_lock(&global_mutex);

    pthread_mutex_t mutex = drm->mutex;

    if (!graph_contains_vertex(rag, thread_id)) {
        pthread_mutex_unlock(&global_mutex);
        return 0; // return w/o unlocking
    }
    if (!graph_contains_vertex(rag, drm)) {
        pthread_mutex_unlock(&global_mutex);
        return 0; // return 1/o unlocking
    }
 
    if (graph_adjacent(rag, drm, thread_id)) {
        graph_remove_edge(rag, drm, thread_id); // remove edge
        pthread_mutex_unlock(&mutex); // unlock the drm
    }

    pthread_mutex_unlock(&global_mutex);
    return 1;
}

int drm_wait(drm_t *drm, pthread_t *thread_id) {
   pthread_mutex_lock(&global_mutex); // CRITICAL

    if (!graph_contains_vertex(rag, drm)) {
        pthread_mutex_unlock(&global_mutex); // END CRITICAL
        return 0;
    }

    graph_add_vertex(rag, thread_id); // add vertex 

    pthread_mutex_t mutex = drm->mutex;
    
    // detect deadlock --> dfs cycle
    // locking own mutex again --> already edge from the resource to the process
    if (graph_adjacent(rag, drm, thread_id)) { // has deadlock
        pthread_mutex_unlock(&global_mutex); // END CRITICAL
        return 0; // return w/o locking drm
    } 
    
    graph_add_edge(rag, thread_id, drm); // request the resource
    if (dfs(thread_id) == 1) {
        graph_remove_edge(rag, thread_id, drm); // remove request edge
        pthread_mutex_unlock(&global_mutex); // END CRITICAL
        return 0; // return w/o locking drm
    }
    
    graph_remove_edge(rag, thread_id, drm); // remove request edge
    graph_add_edge(rag, drm, thread_id); // add using resource edge
    pthread_mutex_unlock(&global_mutex); // END CRITICAL

    pthread_mutex_lock(&mutex); // lock the drm

    return 1;
}

// resource: https://courses.cs.washington.edu/courses/cse326/03su/homework/hw3/dfs.html
int dfs(void* ragNode) {
    if (resetSet == 0) {
        nodes = shallow_set_create();
    }

    if (!set_contains(nodes, ragNode)) { // not visited
        set_add(nodes, ragNode);
        vector* adjacent = graph_neighbors(rag, ragNode);
        for (size_t i = 0; i < vector_size(adjacent); i++) {
            if (dfs((void*)vector_get(adjacent, i))) {
                resetSet = 1; // reset
                return 1;
            }
        }
        
    } else { // already visited --> cycle
        resetSet = 1; // reset
        return 1;
    }

    // there's no cycle:
    nodes = NULL;
    return 0;
}

void drm_destroy(drm_t *drm) {
    pthread_mutex_destroy(&(drm->mutex));

    vector* adjacent = graph_neighbors(rag, drm);
    for (size_t i = 0; i < vector_size(adjacent); i++) {
        graph_remove_edge(rag, drm, (pthread_t*)vector_get(adjacent, i)); // remove the edges
    }
    graph_remove_vertex(rag, drm); // remove the node
    

    if (graph_vertex_count(rag) == 0) {
        graph_destroy(rag);
    }
    free(drm);
    pthread_mutex_destroy(&global_mutex);
    return;
}
