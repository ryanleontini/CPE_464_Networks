#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "handletable.h"

Handle *handleTable = NULL;
size_t handleTableCapacity = 0;
size_t handleTableSize = 0;
int numValid = 0;

void printHandleInHex(const char* handle);

void initializeHandleTable(size_t initialSize) {
    handleTableCapacity = initialSize;
    handleTableSize = initialSize;
    handleTable = malloc(handleTableCapacity * sizeof(Handle));
    if (handleTable == NULL) {
        perror("Failed to create Handle Table.");
        exit(EXIT_FAILURE);
    }
    memset(handleTable, 0, handleTableCapacity * sizeof(Handle)); /* Set all handles to zero. */
}

void freeHandleTable() {
    for (size_t i = 0; i < handleTableSize; i++) {
        if (handleTable[i].validFlag) {
            free(handleTable[i].handle);
        }
    }
    free(handleTable);
    handleTable = NULL;
    handleTableCapacity = 0;
    handleTableSize = 0;
}

void resizeHandleTable() {
    size_t newCapacity = handleTableCapacity * 2;
    Handle *newTable = realloc(handleTable, newCapacity * sizeof(Handle));
    if (newTable == NULL) {
        perror("Failed to resize Handle Table.");
        freeHandleTable();
        exit(EXIT_FAILURE);
    }
    handleTable = newTable;
    memset(handleTable + handleTableCapacity, 0, (newCapacity - handleTableCapacity) * sizeof(Handle));
    handleTableCapacity = newCapacity;
    handleTableSize = newCapacity;
}

int addHandle(int socketNum, char * handle) {
    if (socketNum >= handleTableCapacity) {
        resizeHandleTable(socketNum + 1);
    }

    /* Check if handle already in table. */
    for (int i = 0; i < handleTableSize; i++) {
        if (handleTable[i].handle != NULL) {
            if (handleTable[i].validFlag && strcmp(handleTable[i].handle, handle) == 0) {
                return -1;
            }
        }
    }


    if (handleTable[socketNum].validFlag) {
        free(handleTable[socketNum].handle);
    }

    handleTable[socketNum].handle = malloc(strlen(handle) + 1);
    if (handleTable[socketNum].handle == NULL) {
        perror("Failed to allocate Handle.");
        return -1;
    }

    strcpy(handleTable[socketNum].handle, handle);
    handleTable[socketNum].validFlag = 1;
    
    numValid++;
    return 0;
}

void removeHandle(size_t socketNum) {
    if (socketNum >= handleTableCapacity) {
        fprintf(stderr, "Error: socketNum out of bounds.\n");
        return;
    }

    if (handleTable[socketNum].validFlag) {
        free(handleTable[socketNum].handle);
        handleTable[socketNum].handle = NULL;
        handleTable[socketNum].validFlag = 0;
    }
}

int findSocket(char * handle) {

    for (size_t i = 0; i < handleTableSize; i++) {
        if (handleTable[i].validFlag && strcmp(handleTable[i].handle, handle) == 0) {
            return (int)i;
        }
    }
    return -1;
}

char * findHandle(size_t socketNum) {
    if (socketNum >= handleTableCapacity) {
        fprintf(stderr, "Error: socketNum out of bounds.\n");
        return -1;
    }

    if (handleTable[socketNum].validFlag) {
        return handleTable[socketNum].handle;
    }
    return NULL;
}

void printHandleTable() {
    printf("Current Handle Table:\n");
    for (size_t i = 0; i < handleTableSize; i++) {
        if (handleTable[i].validFlag) {
            printf("Index %zu: Handle = '%s'\n", i, handleTable[i].handle);
        }
    }
}

void printHandleInHex(const char* handle) {
    while (*handle) {
        printf("%02X ", (unsigned char) *handle++);
    }
    printf("\n");
}

int32_t getValids(void) {
    return numValid;
}

size_t getHandleTableSize(void) {
    return handleTableSize;
}

const Handle* getHandleAtIndex(size_t index) {
    if (index >= handleTableSize) {
        return NULL;
    }
    return &handleTable[index];
}