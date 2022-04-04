/**
 * Mini Memcheck
 * CS 241 - Spring 2020
 */
#include "mini_memcheck.h"
#include <stdio.h>
#include <string.h>

// partner : nehap2

size_t total_memory_requested;
meta_data* head;
size_t total_memory_freed;
size_t invalid_addresses;

void *mini_malloc(size_t request_size, const char *filename,
                  void *instruction) {
    if (request_size == 0) return NULL;

    void* newMem = malloc(request_size + sizeof(meta_data));
    if (newMem == NULL) return NULL;

    meta_data* new_meta = newMem;
    new_meta->filename = filename;
    new_meta->request_size = request_size;
    new_meta->instruction = instruction;
    
    // insert new node at the head
    new_meta->next = head;
    head = new_meta;
    
    total_memory_requested += request_size;

    newMem += sizeof(meta_data);
    return newMem;
}

void *mini_calloc(size_t num_elements, size_t element_size,
                  const char *filename, void *instruction) {
    if (num_elements == 0 || element_size == 0) {
        return NULL;
    }
    
    void* newMem = mini_malloc(num_elements * element_size, filename, instruction);
    if (newMem == NULL) {
        return NULL;
    }
    
    // need to initialize newMem to default values
    memset(newMem, 0, num_elements * element_size);

    return newMem;
}

void *mini_realloc(void *payload, size_t request_size, const char *filename,
                   void *instruction) {
    if (payload == NULL) {
        void* newMem = mini_malloc(request_size, filename, instruction);
        return newMem;
    }

    if (request_size == 0) {
        mini_free(payload);
        return NULL;
    }

    if (request_size == 0 && payload == NULL) return NULL;

    meta_data* meta_ptr = (meta_data*)(payload - sizeof(meta_data));

    // check if invalid
    meta_data* temp = head;
    meta_data* previous = NULL;
    int valid = 0;
    while (temp != NULL) {
        if (temp == meta_ptr) {
            if (previous != NULL) {
                previous->next = temp->next;
            } else {
                head = temp->next;
            }
            valid = 1;
            break;
        }
        previous = temp;
        temp = temp->next;
    }

    if (valid == 0) {
        invalid_addresses++;
        return NULL;
    }

    if (meta_ptr->request_size < request_size) {
        // expanding memory
        total_memory_requested += (request_size - meta_ptr->request_size);
    } else if (meta_ptr->request_size > request_size) {
        // decreasing memory
        total_memory_freed += (meta_ptr->request_size - request_size);
    } else {
        meta_ptr->filename = filename;
        meta_ptr->instruction = instruction;
        meta_ptr->request_size = request_size;
        return payload;
    }

    void* j = realloc(meta_ptr, request_size + sizeof(meta_data));
    meta_data* new_meta = (meta_data*)j;
    new_meta->next = head;
    head = new_meta;
    
    // update the existing allocation
    new_meta->filename = filename;
    new_meta->instruction = instruction;
    new_meta->request_size = request_size;
    
    void* newMem = j + sizeof(meta_data);
    return newMem;
}

void mini_free(void *payload) {
    if (payload == NULL) return;

    meta_data* meta_ptr = (meta_data*)(payload - sizeof(meta_data));
    size_t freeBytes = meta_ptr->request_size;
    
    meta_data* temp = head;
    meta_data* previous = NULL;

    while (temp != NULL) {
        if (temp == meta_ptr) {
            if (previous != NULL) {
                previous->next = temp->next;
            } else {
                head = temp->next;
            }
            free(temp);
            total_memory_freed += freeBytes;
            return;
        }
        previous = temp;
        temp = temp->next;
    }

    invalid_addresses++;
    return;

}
