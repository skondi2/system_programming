/**
 * Extreme Edge Cases
 * CS 241 - Spring 2020
 */
#include "camelCaser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

char **camel_caser(const char *input_str) {
    // TODO: Implement me!
    if (input_str == NULL) {
        return NULL;
    }

    int length = strlen(input_str);

    int numOutputs = 0;
    for (int i = 0; i < length; i++) {
        if (ispunct(input_str[i])) numOutputs++;
    }

    if (numOutputs == 0) {
        char** nullResult = (char**) malloc(sizeof(NULL) + 1);
        nullResult[0] = NULL;
        return nullResult;
    } 
    
    int phraseLengths[numOutputs]; // create array of sentence lengths

    int phraseCount = 0;
    int charCount = 0;
    for (int i = 0; i < length; i++) {
        if (!ispunct(input_str[i])) {
            if (!isspace(input_str[i])) {
                charCount++;
            }
        } 
        else {
            phraseLengths[phraseCount] = charCount;
            charCount = 0; // reset charCount
            phraseCount++;
        }
    }
    
    char** output_s = (char**) malloc(sizeof(char*) * (numOutputs+1) + 1);

    if (!output_s) {
        // THEN YOU'VE FAILED!!
        return NULL;
    }

    int sentencePtr = 0;
    int tempCount = 0;
    int i = 0;
    int de = 1; // true
    while (i < length) {
        // make outputTemp on the heap:
        char* outputTemp = (char*) malloc(sizeof(char) * phraseLengths[sentencePtr] + 1);

        if (!outputTemp) {
            // THEN YOU'VE FAILED!!
            return NULL; // IS THIS CORRECT RETURN?
        }

        int j = i;
        while (!ispunct(input_str[j]) && j < length) {
            if (!isspace(input_str[j]) && isalpha(input_str[j])) {
                if (de != 1) { // DO CAMEL CASING!
                    if (isspace(input_str[j - 1])) { // previous char was a space, capitalize
                        outputTemp[tempCount] = toupper(input_str[j]);
                    } else { 
                        outputTemp[tempCount] = tolower(input_str[j]);
                    }
                } else {
                    outputTemp[tempCount] = tolower(input_str[j]);
                    de = 0;
                }
                tempCount++;
            } else if (!isspace(input_str[j]) && !isalpha(input_str[j])) {
                outputTemp[tempCount] = input_str[j];
                tempCount++;
            }
            j++;
        }
        outputTemp[tempCount] = (char) NULL;
        i = j+1;
        tempCount = 0; // reset tempCount
        output_s[sentencePtr] = outputTemp;
        // want default camel ON
        de = 1;

        sentencePtr++;
    }     

    output_s[numOutputs] = (char*)NULL;
    return output_s;

}

void destroy(char **result) {
    // Need to free output_s and the outputTemps in it!!
    if (result == NULL) return;

    int resultsNum = 0;
    for (;result[resultsNum] != NULL; resultsNum++);

    for (int i = 0; i < resultsNum; i++) {
        free(result[i]);
    }

    free(result);
    result = NULL;

    return ;
}