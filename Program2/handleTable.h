#ifndef HANDLETABLE_H
#define HANDLETABLE_H

#include <stdint.h>

void initializeHandleTable(size_t initialSize);
void freeHandleTable();
void resizeHandleTable();
int addHandle (int socketNum, char * handle);
void removeHandle (size_t socketNum);
int findSocket (char * handle);
char * findHandle (int socketNum);
void printHandleTable();

#endif