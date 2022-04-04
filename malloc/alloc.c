/**
 * Malloc
 * CS 241 - Spring 2020
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>

typedef struct struct_metadata {
    int is_free;
    size_t size;

    struct struct_metadata* prev;
    struct struct_metadata* next;

    struct struct_metadata* prevFree;
    struct struct_metadata* nextFree;
} metadata;

void* malloc_freed_block(metadata* temp, size_t desiredSize);
void* malloc_split_block(metadata* temp, size_t desiredSize);
void prev_double_coalesce(metadata* prev, metadata* temp, metadata* next);
void triple_coalesce(metadata* prev, metadata* temp, metadata* next);
void next_double_coalesce(metadata* prev, metadata* temp, metadata* next);
int checkToCoalesce(metadata* temp);

static metadata* head = NULL;
static metadata* tail = NULL;
static metadata* headFree = NULL;


int force_printf(const char *format, ...) {
    static char buf[4096]; //fine because malloc is not multithreaded

    va_list args;
    va_start(args, format);
    vsnprintf(buf, 4096, format, args);
    buf[4095] = '\0'; // to be safe
    va_end(args);
    write(1, buf, strlen(buf));
    return 0;
}

// helper function to print out list
void print_list() {
    //fprintf(stderr, "sizeof(meta): %lu\n", sizeof(metadata));
    if (headFree == NULL) force_printf("head free is NULL\n");
    if (head == NULL) force_printf("head is NULL\n");

    metadata* temp = head;
    fprintf(stderr, "this is the head : %p\n", head);
    fprintf(stderr, "this is the headFree : %p\n", headFree);
    fprintf(stderr, "------------------------\n");
    while (temp != NULL) {
        fprintf(stderr, "this is temp : %p\n", temp);
        fprintf(stderr, "this is temp->size : %lu\n", temp->size);
        fprintf(stderr, "is it free? : %d\n", temp->is_free);
        fprintf(stderr, "this is temp->next : %p\n", temp->next);
        fprintf(stderr, "this is temp->nextFree : %p\n", temp->nextFree);
        fprintf(stderr, "this is temp->prev : %p\n", temp->prev);
        fprintf(stderr, "this is temp->prevFree : %p\n", temp->prevFree);

        temp = temp->next;
    }

}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) return NULL; // undefined behavior

    void* newMem = malloc(num * size);
    if (newMem == NULL) return NULL; // failed to allocate memory

    memset(newMem, 0, num*size);

    return newMem;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size) {
    if (size == 0) return NULL;

    metadata* temp = headFree;
    metadata* chosen = NULL;
    while (temp != NULL) {
        if (temp->is_free == 0) {
            //force_printf("smth wrong.. nextFree ptrs are off\n"); // this occurs during malloc split
        }
        if (temp->is_free && temp->size >= size) {
            if (chosen == NULL || chosen->size > temp->size) {
                chosen = temp;
                break;
            }
        }
        temp = temp->nextFree;
        //force_printf("incrementing bitch\n");
    }

    if (chosen) {
        return malloc_freed_block(chosen, size);
    }

    void* data = sbrk(sizeof(metadata) + size); // casting to int allocates outside heap for test 11
    if (data == (void*)-1) return NULL; // failed to allocate

    metadata* meta_data = (metadata*)data;
    meta_data->is_free = 0;
    meta_data->size = size;
    meta_data->prev = tail;
    meta_data->next = NULL;
    if (tail != NULL) {
        tail->next = meta_data;
    } 
    tail = meta_data;

    meta_data->nextFree = NULL;
    meta_data->prevFree = NULL;

    return ((void*)((char*)meta_data + sizeof(metadata)));
}

/*
Helper function to remove block from freed list
Only manipulates free pointers
*/
void* malloc_freed_block(metadata* temp, size_t desiredSize) {
    // remove temp from freed list
    metadata* prevTemp = temp->prevFree;
    metadata* nextTemp = temp->nextFree;
    temp->prevFree = NULL;
    temp->nextFree = NULL;

    if (nextTemp != NULL) {
        nextTemp->prevFree = prevTemp;
    }

    if (prevTemp != NULL) {
        prevTemp->nextFree = nextTemp;
    }

    if (temp == headFree) {
        headFree = nextTemp;
    }
    //

    if (temp->size > desiredSize + sizeof(metadata) + 64) { 
        return malloc_split_block(temp, desiredSize);
    } 
    temp->is_free = 0;
    return (void*)((char*)temp + sizeof(metadata));
}


void* malloc_split_block(metadata* temp, size_t desiredSize) {
    metadata* new_meta = (metadata*)((char*)temp + sizeof(metadata) + desiredSize);
    
    new_meta->size = temp->size - sizeof(metadata) - desiredSize; 
    temp->size = desiredSize;

    new_meta->next = temp->next;
    if (temp->next != NULL) {
        temp->next->prev = new_meta;
    }

    new_meta->is_free = 1;
    temp->is_free = 0;

    new_meta->prev = temp;
    temp->next = new_meta;

    // insert new_meta into freed list
    new_meta->prevFree = NULL;
    new_meta->nextFree = headFree;

    if (headFree != NULL) {
        headFree->prevFree = new_meta;
    }
    headFree = new_meta;

    return (void*)((char*)temp + sizeof(metadata));

}


void triple_coalesce(metadata* prev, metadata* temp, metadata* next) {
    // remove next from freed list
    metadata* prevFree = next->prevFree;
    metadata* nextFree = next->nextFree;
    
    if (prevFree != NULL) {
        prevFree->nextFree = nextFree;
    }

    if (nextFree != NULL) {
        nextFree->prevFree = prevFree;
    }

    if (next == headFree) {
        headFree = nextFree;
    }
    //
    prev->size = prev->size + temp->size + next->size + 2*sizeof(metadata);

    if (next == tail) {
        tail = prev;
    }

    metadata *nextNext = next->next;
    if (nextNext) {
        nextNext->prev = prev;
    }
    prev->next = nextNext;

    next->prevFree = NULL;
    next->nextFree = NULL;

}

void prev_double_coalesce(metadata* prev, metadata* temp, metadata* next) {
    if (temp == tail) {
        tail = prev;
    }

    prev->size = prev->size + temp->size + sizeof(metadata);

    if (next != NULL) {
        next->prev = prev;
    }
    prev->next = next;

}


void next_double_coalesce(metadata* prev, metadata* temp, metadata* next) {  
    // remove next from freed list
    metadata* prevFree = next->prevFree;
    metadata* nextFree = next->nextFree;

    if (nextFree != NULL) {
        nextFree->prevFree = prevFree;
    }

    if (prevFree != NULL) {
        prevFree->nextFree = nextFree;
    }

    if (next == headFree) {
        headFree = nextFree;
    }
    //
    temp->size = temp->size + sizeof(metadata) + next->size;
    temp->is_free = 1;
    
    metadata *nextNext = next->next;
    if (nextNext) {
        nextNext->prev = temp;
    }
    temp->next = nextNext;

    next->prevFree = NULL;
    next->nextFree = NULL;

    // insert updated temp into freed list
    temp->prevFree = NULL;
    temp->nextFree = headFree;

    if (headFree != NULL) {
        headFree->prevFree = temp;
    }
    headFree = temp;

}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {

    if (ptr == NULL) {
        return;
    }

    metadata *meta = (metadata *) ((void*)(char*)ptr - sizeof(metadata));

    if (meta->is_free == 1) { // don't double free
        return;
    }
    int merged = checkToCoalesce(meta);

    if (!merged) {
        meta->is_free = 1;
        
        meta->prevFree = NULL;
        meta->nextFree = headFree;

        if (headFree != NULL) {
            headFree->prevFree = meta;
        }

        headFree = meta;
    }

}

// Helper function to check if can blend free blocks
int checkToCoalesce(metadata* temp) {

    metadata *next_meta = (metadata *) temp->next;
    metadata *prev_meta = (metadata *) temp->prev;

    if (next_meta && next_meta->is_free && prev_meta && prev_meta->is_free && ((void *) prev_meta) + sizeof(metadata) + prev_meta->size == temp && 
        ((void *) next_meta) - sizeof(metadata) - temp->size == temp) {
        triple_coalesce(prev_meta, temp, next_meta);
    
        return 1;
    }
    if (prev_meta && prev_meta->is_free && ((void *) prev_meta) + sizeof(metadata) + prev_meta->size == temp) {
        prev_double_coalesce(prev_meta, temp, next_meta);
        
        return 1;
    }

    if (next_meta && (next_meta != tail) && next_meta->is_free && 
        ((void *) next_meta) - sizeof(metadata) - temp->size == temp) {
        next_double_coalesce(prev_meta, temp, next_meta);
        
        return 1;
    }
    
    return 0;
}


/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }
    metadata* newPtr = (metadata*)(ptr - sizeof(metadata));
    size_t oldSize = newPtr->size;

    if (oldSize >= size) {
        return ptr;
    }
    
    void* new = malloc(size);
    
    if (new == NULL) {
        return NULL;
    }
    size_t min = oldSize;
    if (size < min) {
        min = size;
    }
    memcpy(new, ptr, min);

    free(ptr);

    return new;
}