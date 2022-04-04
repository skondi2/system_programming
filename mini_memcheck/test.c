/**
 * Mini Memcheck
 * CS 241 - Spring 2020
 */
#include <stdio.h>
#include <stdlib.h>

int main() {
    // Your tests here using malloc and free
    void *p1 = malloc(20);
    void *p2 = malloc(40);
    void *p3 = malloc(50);
    //free(p1);
    //free(p2);
    //free(p3);

    p1 = realloc(p1, 50);
    p2 = realloc(p2, 80);
    p3 = realloc(p3, 20);

    //free(p5);
    //free(p6);
    //free(p4);
    free(p2);
    free(p1);
    free(p3);
    return 0;
}

