/**
 * Perilous Pointers
 * CS 241 - Spring 2020
 */
#include "part2-functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * (Edit this function to print out the "Illinois" lines in
 * part2-functions.c in order.)
 */
int main() {
    // your code here
    first_step(81);

    int val = 132;
    second_step(&val);

    int val1 = 8942;
    int* pVal1 = & val1;
    double_step(&pVal1);

    char value[6];
    *(int *)(value + 5) = 15;
    strange_step(value);

    char value1[4];
    ((char *)value1)[3] = 0;
    empty_step(value1);

    char s2[4];
    s2[3] = 'u';
    two_step(s2, s2);

    char x = 'a';
    char* xPtr = &x;
    three_step(xPtr, xPtr + 2, xPtr + 4);

    char first[2];
    first[0] = 0;
    first[1] = 0;
    char second[3]; 
    second[0] = 0;
    second[1] = 0;
    second[2] = 8;
    char third[4];
    third[0] = 0;
    third[1] = 0;
    third[2] = 8;
    third[3] = 16;
    step_step_step(first, second, third);

    int b = 9;
    char* bPtr = (char*) &b;
    it_may_be_odd(bPtr, b);

    char str[] = "hi,CS241,h";
    tok_step(str);

    int num = 513;
    int* orange = &num;
    the_end((void*)orange, (void*)orange);
    return 0;
}
