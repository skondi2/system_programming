/**
 * Vector
 * CS 241 - Spring 2020
 */
#include "sstring.h"
#include "vector.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>

struct sstring {
    char* value;
    size_t size;
    size_t capacity;
};

sstring *cstr_to_sstring(const char *input) {
    sstring* newSstring = malloc(sizeof(sstring));
    size_t inputLen = strlen(input);
    char* newValue = malloc( (inputLen+1) * sizeof(char) );
    for (size_t i = 0; i < inputLen; i++) {
        newValue[i] = input[i];
    }
    newValue[inputLen] = '\0';
    newSstring->value = newValue;
    newSstring->size = inputLen;
    newSstring->capacity = 16;

    return newSstring;
}

char *sstring_to_cstr(sstring *input) {
    size_t inputLen = strlen(input->value);
    char* newString = malloc( sizeof(char) * (inputLen+1) );
    for (size_t i = 0; i < strlen(input->value); i++) {
        newString[i] = input->value[i];
    }
    newString[strlen(input->value)] = '\0';
    return newString;
}

int sstring_append(sstring *this, sstring *addition) {
    char* thisValue = this->value;
    char* additionValue = addition->value;

    int len1 = strlen(thisValue);
    int len2 = strlen(additionValue); 
    this->value = realloc(this->value, len1 + len2 + 1);
    thisValue = this->value;
    char* appended = strcat(thisValue, additionValue);
    this->value = appended;
    this->size = len1 + len2;

    return len1 + len2;
}

vector *sstring_split(sstring *this, char delimiter) {
    char* thisCpy = malloc(sizeof(char) * strlen(this->value) + 1);
    thisCpy = strcpy(thisCpy, this->value);
    vector* result = vector_create(string_copy_constructor, string_destructor, 
        string_default_constructor);
    char* original = thisCpy;
    char* token = strsep(&original, &delimiter);

    while (token) {
        vector_push_back(result, token);
        token = strsep(&original, &delimiter);
    }
    
    free(thisCpy);
    return result;
}
/* citation:
https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-
word/?fbclid=IwAR37J3j9Lxzk2FsLhF6ffFMFUPSCIH3xKxOKXwBHowNmQMsdUmW4jaY3Zx8
*/
int sstring_substitute(sstring *this, size_t offset, char *target,
                       char *substitution) {
    
    char* manipulateMe = malloc(sizeof(char) * (strlen(this->value)+1));
    manipulateMe = strcpy(manipulateMe, this->value);

    size_t j = 0;
    size_t count = 0;

    size_t targetLen = strlen(target);
    size_t subLen = strlen(substitution);
    size_t maniLen = strlen(manipulateMe);

    for ( ; j < maniLen; j++ ) {
        if (manipulateMe[j] == '\0') break;

        if (strstr(manipulateMe, target)) {
            count++;
            j += targetLen - 1;
        }
    }
    
    char* result = malloc(sizeof(char) * (j + count * (subLen - targetLen) + 1));
    j = 0;
    char* start = manipulateMe;

    while (*start) {
        if (j >= offset && strstr(start, target) == start) {
            strcpy(&result[j], substitution); 
            j += subLen; 
            start += targetLen;
        } else {
            result[j++] = *start++; 
        }
    }

    result[j] = '\0'; 
    this->value = realloc(this->value, sizeof(char) * (1+strlen(result)));
    for (size_t i = 0; i < strlen(result); i++) {
        this->value[i] = result[i];
    }
    this->value[strlen(result)] = '\0';
    free(result);
    free(manipulateMe);
    return 0; 
}


char *sstring_slice(sstring *this, int start, int end) {
    // make copy of the sstring
    char* buff = malloc(sizeof(char) * (end - start + 1));
    memcpy(buff, &((this->value)[start]), end - start);
    buff[end - start] = '\0';
    return buff;
}

void sstring_destroy(sstring *this) {
    this->size = 0;
    this->capacity = 0;

    free(this->value);

    free(this);
}
