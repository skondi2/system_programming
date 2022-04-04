/**
 * Mad Mad Access Patterns
 * CS 241 - Spring 2020
 */
#include "tree.h"
#include "utils.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

/*
  Look up a few nodes in the tree and print the info they contain.
  This version uses mmap to access the data.

  ./lookup2 <data_file> <word> [<word> ...]
*/

void traverseTree(char* desiredWord, int offset, char* file_data);

int main(int argc, char **argv) {
    if (argc <= 2) {
        printArgumentUsage();
        exit(1);
    }

    int file = open(argv[1], O_RDONLY);    
    if (file == -1) {
        //printf("failed to open the file \n");
        openFail(argv[1]);
        exit(2);
    }

    struct stat sb;
    if (fstat(file, &sb) == -1) {
        openFail(argv[1]);
        exit(2);
    }

    char* file_data = (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file, 0);
    if ((void*)file_data == MAP_FAILED) {
        mmapFail(argv[1]);
        exit(2);
    }
    close(file);

    // check BTRE:
    if (strncmp(file_data, BINTREE_HEADER_STRING, 4) != 0) { 
        //printf("btre failed \n");
        formatFail(argv[1]);
        exit(2);
    }

    for (int i = 2; i < argc; i++) {
        char* word = argv[i];
        int startingOffset = BINTREE_ROOT_NODE_OFFSET; 

        traverseTree(word, startingOffset, file_data);
    }

  
    return 0;
}

void traverseTree(char* desiredWord, int offset, char* file_data) {
    if (offset == 0) {
      printNotFound(desiredWord); // couldn't find the word
      return;
    }

    BinaryTreeNode* wordNode = (BinaryTreeNode*)(file_data + offset);

    if (strcmp(desiredWord, wordNode->word) < 0) {
        // traverse left
        traverseTree(desiredWord, wordNode->left_child, file_data);
    } else if (strcmp(desiredWord, wordNode->word) > 0) {
        // traverse right
        traverseTree(desiredWord, wordNode->right_child, file_data);
    } else {
        printFound(desiredWord, wordNode->count, wordNode->price);
        return;
    }
}