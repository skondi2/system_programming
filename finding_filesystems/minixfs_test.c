/**
 * Finding Filesystems
 * CS 241 - Spring 2020
 */
#include "minixfs.h"
#include "minixfs_utils.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    // Write tests here!
    //char * buffer = calloc(1, 13);
    file_system *fs = open_fs("test.fs");
    off_t off = 0;
    char buf[10000];
    ssize_t bytes_read = minixfs_read(fs, "/goodies/hello_filesystems.txt", buf, 10000, &off);
    fprintf(stderr, "bytes read is %lu \n", bytes_read);
    fprintf(stderr, "bytes in file are : %s \n", buf);
    //char *expected[13];
    //open /goodies/hello.txt with open() or fopen() and read the contents way you normally would
    //assert(!strcmp(buf, expected));
    close_fs(&fs);
}