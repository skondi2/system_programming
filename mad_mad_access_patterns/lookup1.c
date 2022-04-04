/**
 * Mad Mad Access Patterns
 * CS 241 - Spring 2020
 */
#include "tree.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
  Look up a few nodes in the tree and print the info they contain.
  This version uses fseek() and fread() to access the data.

  ./lookup1 <data_file> <word> [<word> ...]
*/
void traverseTree(char* desiredWord, int offset, char* fileName, FILE* filePtr);

int main(int argc, char **argv) {
  if (argc <= 2) {
    printArgumentUsage();
    exit(1);
  }
  
  FILE* file = fopen(argv[1], "r");
  if (file == NULL) {
    openFail(argv[1]);
    exit(2);
  }
  
  // check BTRE:
  char firstBytes[5];
  fread(firstBytes, 4, 1, file);
  firstBytes[4] = '\0';
  if (strncmp(firstBytes, BINTREE_HEADER_STRING, 4) != 0) { // b/c BTRE isn't null-terminated
    formatFail(argv[1]);
    exit(2);
  }

  for (int i = 2; i < argc; i++) {
    char* word = argv[i];
    int startingOffset = BINTREE_ROOT_NODE_OFFSET; 

    traverseTree(word, startingOffset, argv[1], file);
  }
  

  fclose(file);
  return 0;
}

void traverseTree(char* desiredWord, int offset, char* fileName, FILE* filePtr) {
  if (offset == 0) {
      printNotFound(desiredWord); // couldn't find the word
      return;
    }

    int canRead = fseek(filePtr, offset, SEEK_SET); // need to read file at that offset
    if (canRead == -1) {
      formatFail(fileName);
      exit(1); 
    }

    BinaryTreeNode wordNode;
    fread(&wordNode, sizeof(BinaryTreeNode), 1, filePtr);

    // need to read whole thing starting from the offset
    int canRead1 = fseek(filePtr, offset + sizeof(BinaryTreeNode), SEEK_SET);
    if (canRead1 == -1) {
      formatFail(fileName);
      exit(1); 
    }

    // read word from the file
    char word[200];
    char* success = fgets(word, 200, filePtr); // will stop at new line
    if (success == NULL) {
      formatFail(fileName);
      exit(1);
    }
    
    // recursively find the correct node
    if (strcmp(desiredWord, word) < 0) {
      // traverse left
      traverseTree(desiredWord, wordNode.left_child, fileName, filePtr);
    } else if (strcmp(desiredWord, word) > 0) {
      // traverse right
      traverseTree(desiredWord, wordNode.right_child, fileName, filePtr);
    } else {
      printFound(desiredWord, wordNode.count, wordNode.price);
      return;
    }
}