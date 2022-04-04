/**
 * Extreme Edge Cases
 * CS 241 - Spring 2020
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camelCaser.h"
#include "camelCaser_tests.h"

int test_camelCaser(char **(*camelCaser)(const char *),
                    void (*destroy)(char **)) {
    // TODO: Implement me!
    int passed = 1; 
    while (1) {
        // 1. NULL check:
        char** nullOutput = camelCaser(NULL);
        if (nullOutput != NULL) {
            passed = 0;
            break;
        }

        // 2. 1 sentence, NULL at the end:
        char** output1 = camelCaser("Hello World.");
        char *expected1[] = {"helloWorld", NULL};
        if (output1 == NULL || strcmp(output1[0], expected1[0])) {
            passed = 0;
            break;
        }
        if (output1[1] != NULL) {
            passed = 0;
            break;
        }

        // 5. 1 sentence, mixed case
        char** output4 = camelCaser("hELlo wORlD.");
        if (output4 == NULL || strcmp(output4[0],expected1[0])) {
            passed = 0;
            break;
        }
        if (output4[1] != NULL) {
            passed = 0;
            break;
        }

        // 6. extra spaces between words
        char** output5 = camelCaser("Hello    world.");
        if (output5 == NULL || strcmp(output5[0], expected1[0])) {
            passed = 0;
            break;
        }
        if (output5[1] != NULL) {
            passed = 0;
            break;
        }

        // 7. leading and trailing whitespaces
        char** output6 = camelCaser("   Hello world   .");
        if (output6 == NULL || strcmp(output6[0], expected1[0])) {
            passed = 0;
            break;
        }
        if (output6[1] != NULL) {
            passed = 0;
            break;
        }

        // 8. "only special characters"
        char** output7 = camelCaser("*4! #.");
        char *expected7[] = {"", "4", "", "", NULL};
        if (output7 == NULL || strcmp(output7[0], expected7[0])) {
            passed = 0;
            break;
        }
        if (strcmp(output7[1], expected7[1])) {
            passed = 0;
            break;
        }  
        if (strcmp(output7[2], expected7[2])) {
            passed = 0;
            break;
        } 
        if (strcmp(output7[3], expected7[3])) {
            passed = 0;
            break;
        } 
        if (output7[4] != NULL) {
            passed = 0;
            break;
        }

        //9. multiple sentences
        char** output8 = camelCaser("Hello world. Bello world.");
        char *expected8[] = {"helloWorld", "belloWorld", NULL};
        if (output8 == NULL || strcmp(output8[0], expected8[0])) {
            passed = 0;
            break;
        }
        if (strcmp(output8[1], expected8[1])) {
            passed = 0;
            break;
        } 
        if (output8[2] != NULL) {
            passed = 0;
            break;
        }

        // 10. input is "."
        /*char** output9 = camelCaser(".");
        char *expected9[] = {"", NULL};
        if (output9 == NULL || strcmp(expected9[0], output9[0])) {
            passed = 0;
            break;
        }
        if (output9[1] != NULL) {
            passed = 0;
            break;
        }*/

        // 11. mix of letters and special characters
        char** output10 = camelCaser("hE(* a#h.");
        char *expected10[] = {"he", "", "a", "h", NULL};
        if (output10 == NULL || strcmp(expected10[0], output10[0])) {
            passed = 0;
            break;
        } 
        if (strcmp(expected10[1], output10[1])) {
            passed = 0;
            break;
        }
        if (strcmp(expected10[2], output10[2])) {
            passed = 0;
            break;
        }
        if (strcmp(expected10[3], output10[3])) {
            passed = 0;
            break;
        }
        if (output10[4] != NULL) {
            passed = 0;
            break;
        }

        // 12. input is "Hello.World" 
        char** output11 = camelCaser(".Hello.World.");
        char *expected11[] = {"", "hello", "world", NULL};
        if (output11 == NULL || strcmp(expected11[0], output11[0])) {
            passed = 0;
            break;
        }
        if (strcmp(expected11[1], output11[1])) {
            passed = 0;
            break;
        }
        if (strcmp(expected11[2], output11[2])) {
            passed = 0;
            break;
        }
        if (output11[3] != NULL) {
            passed = 0;
            break;
        }

        // 13. empty string should return NULL
        char** output13 = camelCaser("");
        // pointer to one element array that contains null
        if (output13 == NULL || *output13 != NULL) {
            passed = 0;
            break;
        } 

        // without period
        char** outputA = camelCaser("hello World");
        if (outputA == NULL || *outputA != NULL) {
            passed = 0;
            break;
        }

        // special backslash case
        char** output14 = camelCaser("\1\b.");
        char* expected56[] = {"\1\b"};
        if (output14 == NULL || strcmp(output14[0], expected56[0])) {
            passed = 0;
            break;
        }
        if (output14[1] != NULL) {
            passed = 0;
            break;
        }

        // null character in the middle :
        char** output15 = camelCaser("hello\0world.");
        if (output15 == NULL || *output15 != NULL) {
            passed = 0;
            break;
        }

        // ascii character case
        char** outputAscii = camelCaser("Â.");
        char* expectedAscii[] = {"Â", NULL};
        if (outputAscii == NULL || strcmp(outputAscii[0], expectedAscii[0])) {
            passed = 0;
            break;
        }

        // empty space character 
        char** output89 = camelCaser(" ");
        if (output89 == NULL || output89[0] != NULL) {
            passed = 0;
            break;
        }


        // string with backslash character
        /*char** output17 = camelCaser("yellow\b.");
        char* expected17[] = {"yellow\b"};
        if (output17 == NULL || strcmp(output17[0], expected17[0])) {
            passed = 0;
            break;
        }
        if (output17[1] != NULL) {
            passed = 0;
            break;
        }*/

        // 14. destroy NULL result
        destroy(output13);
        destroy(outputA);
        destroy(output14);

        // 15. destroy test on non-NULL results
        //destroy(output17);
        destroy(output11);
        destroy(output10);
        //destroy(output9);
        destroy(output8);
        destroy(output7);
        destroy(output6);
        destroy(output5);
        destroy(output4);
        destroy(output89);
        destroy(output15);
        destroy(output1);
        destroy(nullOutput);
        destroy(outputAscii);

        // string starting with a number
        char** output16 = camelCaser("56hello. and 998hy.");
        char *expected72[] = {"56hello", "and998Hy"};
        if (output16 == NULL || strcmp(output16[0], expected72[0])) {
            //printf("here1 \n");
            passed = 0;
            break;
        }
        if (output16[2] != NULL) {
            //printf("here2 \n");
            passed = 0;
            break;
        }
        char temp = *output16[1];
        destroy(output16);
        if (strcmp(&temp, expected72[1])) {
            //printf("here3 \n");
            passed = 0; // fails here 
            break;
        }

        break;
    }

    return passed;
}
