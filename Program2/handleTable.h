#ifndef HANDLETABLE_H
#define HANDLETABLE_H

#include <stdint.h>

typedef struct {
    int validFlag; /* Server has accepted client? */
    char* handle; /* 100 character handle + null terminator. */
} Handle;

void initializeHandleTable(size_t initialSize);
void freeHandleTable();
void resizeHandleTable();
int addHandle (int socketNum, char * handle);
void removeHandle (size_t socketNum);
int findSocket (char * handle);
char * findHandle(size_t socketNum);
void printHandleTable();
size_t getHandleTableSize(void);
int32_t getValids(void);
const Handle* getHandleAtIndex(size_t index);

#endif