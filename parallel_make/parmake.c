/**
 * Parallel Make
 * CS 241 - Spring 2020
 */
// part 2 version code
#include "format.h"
#include "graph.h"
#include "parmake.h"
#include "parser.h"
#include "includes/set.h"
#include "includes/queue.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>

typedef struct node_properties {
    char* name;
    vector* dependencies;
} node_info;

// for cycle detection
set* cycle_set = NULL;

// for adding to queue
queue* node_queue;
set* added_nodes;
set* executed_nodes;
set* failed_nodes;
size_t queue_size = 0;

// for executing rules after deleting node in rag
graph* orig_rag;

// array of threads
pthread_t* threads;

// mutex and conditional variable
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

size_t num_threads_p;
size_t waiting = 0;

// helper functions
int run_command(char* orig, char* lowest_most, vector* dependencies);
int dfs_cycle_detect(char* node, graph* rag);
bool rule_exist(char *command);
bool change_timestamp(char* commandPar, char* commandChild);
void create_queue(graph* rag, vector* filtered, size_t num_threads);
void* execute_nodes(void* param);


int parmake(char *makefile, size_t num_threads, char **targets) {
    threads = malloc(sizeof(pthread_t) * num_threads);
    num_threads_p = num_threads;

    graph* rag = parser_parse_makefile(makefile, targets); 
    orig_rag = parser_parse_makefile(makefile, targets);

    vector* goal_rules = graph_neighbors(rag, "");
    vector* filtered = shallow_vector_create();
    
    added_nodes = string_set_create();
    executed_nodes = string_set_create();
    failed_nodes = string_set_create();
    node_queue = queue_create(-1);

    // remove sentinel vertex from after getting the goals b/c don't want to run it
    graph_remove_vertex(rag, "");
    graph_remove_vertex(orig_rag, "");
    int cycleExists = 0;
    for (size_t i = 0; i < vector_size(goal_rules); i++) {
        char* goal_rule_string = (char*)vector_get(goal_rules, i);

        int bad = 0;
        // if goal rule is in cycle --> print_cycle_failure() on goal_rule
        // Condition 1: CANNOT RUN goal_rule
        if (dfs_cycle_detect(goal_rule_string, rag)) {
            set_destroy(cycle_set);
            cycle_set = NULL;
            bad = 1;
        }
        
        // detect if any of goal rules descendants are in cycle --> dfs to get all its descendants 
        vector* goal_descend = graph_neighbors(rag, goal_rule_string);
    
        for (size_t j = 0; j < vector_size(goal_descend); j++) {
            char* descend_string = (char*)vector_get(goal_descend, j);
            // if descend_rule is in a cycle --> print_cycle_failure() on goal_rule
            // Condition 2: CANNOT RUN goal_rule
            if (dfs_cycle_detect(descend_string, rag)) {
                if (cycle_set != NULL) {
                    set_destroy(cycle_set);
                    cycle_set = NULL;
                }
                bad = 1;
            }
        }

        if (!bad) {
            vector_push_back(filtered, goal_rule_string);
        } else {
            print_cycle_failure(goal_rule_string);
            cycleExists = 1;
        }

        vector_destroy(goal_descend);
    }
    if (!cycleExists) {
        create_queue(rag, filtered, num_threads); 
        if (queue_size < num_threads) {
            num_threads_p = queue_size;
        }
        for (size_t i = 0; i < num_threads_p; i++) {
            pthread_create(&threads[i], NULL, (void*)execute_nodes, NULL);
        }

        for (size_t i = 0; i < num_threads_p; i++) {

            pthread_join(threads[i], NULL);
        }
    }
    //printf("after joining all the threads \n");

    graph_destroy(rag);
    graph_destroy(orig_rag);
    vector_destroy(filtered);
    vector_destroy(goal_rules);
    set_destroy(added_nodes);
    queue_destroy(node_queue);
    set_destroy(executed_nodes);
    set_destroy(failed_nodes);
    free(threads);
    return 0;
}

/**
 * Helper function to create thread queue with tree traversal algorithm
 * 
 * Don't need to lock stuff b/c threads don't enter here
 **/
void create_queue(graph* rag, vector* filtered, size_t num_threads) {
    // filtered has goals that can be run --> now check if their commands can be run

    // do bfs traversal on every subtree
    for (size_t k = 0; k < vector_size(filtered); k++) {
        vector* visited = string_vector_create();
        queue* bfs = queue_create(-1);

        char* node = (char*)vector_get(filtered, k);
        //printf("THIS IS GOAL RULE : %s \n", node);

        //set_add(visited, node); // mark as visited
        vector_push_back(visited, node);
        queue_push(bfs, node);
        size_t bfs_size = 1;

        while (bfs_size != 0) {
            char* curr_node = queue_pull(bfs);
            //printf("current node is %s \n", curr_node);
            bfs_size--;
            vector* adj = graph_neighbors(rag, curr_node);
            for (size_t i = 0; i < vector_size(adj); i++) {
                //if (set_contains(visited, (char*)vector_get(adj, i)) == false) {
                    //set_add(visited, (char*)vector_get(adj, i));
                    vector_push_back(visited, (char*)vector_get(adj, i));
                    queue_push(bfs, (char*)vector_get(adj, i));
                    bfs_size++;
                //}
            }
        }

        size_t size = vector_size(visited);
        while (size != 0) {
            char* t = vector_get(visited, size-1);
            size--;
            vector* adj = graph_neighbors(rag, t);
            vector* adj_copy = shallow_vector_create();
    
            for (size_t i = 0; i < vector_size(adj); i++) {
                vector_push_back(adj_copy, strdup((char*)vector_get(adj, i))); // NEED TO FREE
            }

            if (!set_contains(added_nodes, t)) {
                node_info* node_info = malloc(sizeof(node_info));
                node_info->name = strdup(t); // NEED TO FREE
                node_info->dependencies = adj_copy;
                set_add(added_nodes, t);
                queue_push(node_queue, node_info);
                //printf("PUSHING TO QUEUE \n");
            }
        }
        //printf("ENDING FOR LOOP ITERATION \n");
    }
    
    vector* elements = set_elements(added_nodes);
    queue_size = vector_size(elements);
    
    /*printf("ELEMENTS SIZE : %lu \n", vector_size(elements));
    for (size_t h = 0; h < vector_size(elements); h++) {
        node_info* g = (node_info*)queue_pull(node_queue);
        printf("node name is %s \n", (char*)g->name);
        vector* gh = g->dependencies;
        for (size_t e = 0; e < vector_size(gh); e++) {
            printf("node dependencies for node %s is %s \n", (char*)g->name, (char*)vector_get(gh, e));
        }
    }*/
}

/**
 * Helper function to execute nodes in the queue in dependency order
 * 
 * Threads will start here
 * 
 * NOTE: set is not thread safe but queue and vector are
 * 
 * Things to lock:
 * - set accesses
 * - queue_size decrement
 * 
 **/
void* execute_nodes(void* param) {
    waiting = 0;
    while (1) {

        size_t orig = queue_size;
        //printf("-------- \n");
        //printf("queue size is %lu for %lu \n", queue_size, pthread_self());
        
        node_info* node = queue_pull(node_queue);
        if (node == NULL) {
            queue_push(node_queue, node);
            break;
        }
        //printf("the node is %s , thread %lu \n", node->name, pthread_self());
        
        vector* depend = node->dependencies;
        int all_depend_ran = 1;

        for (size_t i = 0; i < vector_size(depend); i++) {
            pthread_mutex_lock(&lock); // LOCK
            int executed = set_contains(executed_nodes, (char*)vector_get(depend, i));
            int failed = set_contains(failed_nodes, (char*)vector_get(depend, i));
            pthread_mutex_unlock(&lock); // UNLOCK

            if (executed == false) {
                // dependency has not been executed
                //printf("dependency not executed : %s \n", (char*)vector_get(depend, i));
                all_depend_ran = 0;
            }

            if (failed == true) {
                // dependency failed in execution
                //pthread_mutex_lock(&lock); // LOCK
                set_add(failed_nodes, node->name);
                pthread_mutex_unlock(&lock); // UNLOCK
                all_depend_ran = 0; 
            }
            pthread_mutex_unlock(&lock); // UNLOCK
        }

        if (all_depend_ran) {
            // all dependencies executed --> can execute this node!
            int curr_child_failed = run_command(node->name, 
                (char*)graph_get_vertex_value(orig_rag, node->name), node->dependencies);
            
            pthread_mutex_lock(&lock); // LOCK
            if (!curr_child_failed) {
                // if that node successfully executed:
                set_add(executed_nodes, node->name);
            } else {
                set_add(failed_nodes, node->name);
            }

            queue_size--; 
            
            pthread_mutex_unlock(&lock); // UNLOCK
        } else {
            // can't execute yet and it's dependencies haven't failed yet
            pthread_mutex_lock(&lock); // LOCK
            if (set_contains(failed_nodes, node->name) == false) {
                queue_push(node_queue, node);
            } else {
                queue_size--;
            }
            pthread_mutex_unlock(&lock); // UNLOCK
        }
        pthread_mutex_lock(&lock); // LOCK
        if (queue_size == 0) {
            queue_push(node_queue, NULL);
            queue_size = -1; // so that only 1 thread will do this
        }
        pthread_mutex_unlock(&lock); // UNLOCK
        
        pthread_mutex_lock(&lock); // LOCK
        while (queue_size == orig) { // if queue size hasn't changed

            pthread_mutex_unlock(&lock); // UNLOCK
            waiting++;
            pthread_mutex_lock(&lock); // LOCK

            //printf("waiting value is %lu \n", waiting);
            if (waiting >= num_threads_p) {

                //printf("unlocking! \n");
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&lock); // UNLOCK

                waiting = 0;
                break;
            } else {
                //printf("waiting \n");
                pthread_cond_wait(&cond, &lock);
            }
        } 
        pthread_mutex_unlock(&lock); // UNLOCK
        
        pthread_mutex_lock(&lock); // LOCK
        if (queue_size != orig) {
            //printf("unlocking \n");
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&lock); // UNLOCK
        
        // end of while loop
    }
    return NULL;
}


/**
 * Helper function to detect cycles in graph
 * 
 * Returns 1 if cycle detected
 * Returns 0 if no cycle detected
 **/
int dfs_cycle_detect(char* node, graph* rag) {
    if (cycle_set == NULL) {
        cycle_set = shallow_set_create();
    }

    if (!set_contains(cycle_set, node)) { // not visited
        set_add(cycle_set, node);
        vector* adj = graph_neighbors(rag, node);
        
        for (size_t m = 0; m < vector_size(adj); m++) {
            char* elem = (char*)vector_get(adj, m);
            if (dfs_cycle_detect(elem, rag)) {
                vector_destroy(adj);
                return 1;
            }
        }
        vector_destroy(adj);
        
    } else { // already visited --> cycle
        return 1;
    }

    // there's no cycle
    set_destroy(cycle_set);
    cycle_set = NULL;
    return 0;
}

/**
 * Helper function to see if can satisfy rule by
 * running it's command
 * 
 * Only run command if :
 *  - rule name is not on disk
 *  - rule doesn't depend on another rule that is on disk
 * 
 * Returns 0 to mark rule as satisfied
 * Returns 1 if command failed.. means that none of the rules under that goal rule
 *  should run
 **/
int run_command(char* orig, char* lowest_most_rule, vector* dependencies) {
    rule_t* rule = (rule_t*)lowest_most_rule;
    int run = 0;
    int rule_on_file = rule_exist(orig);

    // need to check if rule is file on disk:
    if (rule_on_file) {
        // check if any dependencies are a file on disk
        for (size_t m = 0; m < vector_size(dependencies); m++) {
            if (rule_exist((char*)vector_get(dependencies, m))) {
                // does dependency have newer modification time?
                if (change_timestamp((char*)vector_get(dependencies, m), orig)) {
                    run = 1;
                }
            }
        }
        if (run == 0) {
            // mark rule as satisfied
            return 0;
        }
    }
    vector* commands = rule->commands;

    if (run || !rule_on_file) {
        for (size_t i = 0; i < vector_size(commands); i++) {
            int status = system((char*)vector_get(commands, i));

            if (status != 0) {
                rule->state = 1; // marking it as failed
                // don't want to run any of the parents commands
                return 1;
            }
        }
    }
    return 0; // mark rule as satisfied
}

/**
 * Helper function to see if rule exists in disk
 * 
 * Credits: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
 **/
bool rule_exist(char *command) {
  struct stat buffer;   
  return (stat (command, &buffer) == 0);
}

bool change_timestamp(char* commandPar, char* commandChild) {
    struct stat bufferPar;
    stat(commandPar, &bufferPar);

    struct stat bufferChild;
    stat(commandChild, &bufferChild);

    time_t child = bufferChild.st_ctime;
    time_t parent = bufferPar.st_ctime;
    
    double diff_t = difftime(child, parent);
    if (diff_t > 0.0) {
        // child is newer
        return false;
    } else {
        // child is older
        return true;
    }

}