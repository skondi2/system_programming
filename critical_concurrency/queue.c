/**
 * Critical Concurrency
 * CS 241 - Spring 2020
 */

//partner : skondi2
#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * This queue is implemented with a linked list of queue_nodes.
 */
typedef struct queue_node {
    void *data;
    struct queue_node *next;
} queue_node;

struct queue {
    /* queue_node pointers to the head and tail of the queue */
    queue_node *head, *tail;

    /* The number of elements in the queue */
    ssize_t size;

    /**
     * The maximum number of elements the queue can hold.
     * max_size is non-positive if the queue does not have a max size.
     */
    ssize_t max_size;

    /* Mutex and Condition Variable for thread-safety */
    pthread_cond_t cv;
    pthread_mutex_t m;
};

queue *queue_create(ssize_t max_size) {
    /* Your code here */

    queue* q = malloc(sizeof(queue));

    /*The function pthread_cond_init() initialises the condition variable 
    referenced by cond with attributes referenced by attr*/

    pthread_cond_init(&(q->cv), NULL);

    /*The pthread_mutex_init() function initialises the mutex referenced 
    by mutex with attributes specified by attr*/

    pthread_mutex_init(&(q->m), NULL);

    q->max_size = max_size;
    q->size = 0; 
    q->head = NULL;
    q->tail = NULL;

    return q;
}

void queue_destroy(queue *this) {
    /* Your code here */

    queue_node* temp = this->head;
    queue_node* temp_2;

    //going through the queue to get rid of each queue_node

    while(temp) {
        
        //uing temp_2 to store the value of temp before we delete to avoid memory loss or mistakes with pointer
        temp_2 = temp;

        //going to next node
        temp = temp->next;

        //deleting the temp_2
        free(temp_2);
    }

    /*The pthread_cond_destroy() function shall destroy the given condition variable specified by cond; 
    the object becomes, in effect, uninitialized. */

    pthread_cond_destroy(&(this->cv));

    /*The pthread_mutex_destroy() function shall destroy the mutex object referenced by mutex; 
    the mutex object becomes, in effect, uninitialized*/

    pthread_mutex_destroy(&(this->m));

    free(this); //need to do this to get rid of memory error
}

void queue_push(queue *this, void *data) {
    pthread_mutex_lock(&this->m);
        while (this->max_size == this->size && this->max_size >=0) {
            pthread_cond_wait(&this->cv, &this->m);
        }
    
    queue_node * temp = malloc(sizeof(queue_node));
    temp->data = data;
    temp->next = NULL;
    if (this->tail == NULL) {
        this->head = temp;
    } else {
        this->tail->next = temp;   
    }
    
    this->tail = temp;
    this->size++;
    pthread_cond_broadcast(&this->cv);    
    pthread_mutex_unlock(&this->m);
}

void *queue_pull(queue *this) {
   /* Your code here */
    pthread_mutex_lock(&this->m);
    //block is while or block in if??
    while (this->size == 0) {
        pthread_cond_wait(&this->cv, &this->m);
    } 
    
    queue_node *node = this->head;

    if (this->size == 1) { 
        this->head = NULL;
        this->tail = NULL;
    } else {
        this->head = node->next;
    }

    void * elem = node->data;

    free(node); 
    this->size--;
    if( this->size< this->max_size && this->max_size>0) {
        pthread_cond_broadcast(&this->cv);

    }

    
    pthread_mutex_unlock(&this->m);
    return elem;
}