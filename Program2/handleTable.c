#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int validFlag; /* Server has accepted client? */
    char* handle; /* 100 character handle + null terminator. */
} Handle;

Handle *handleTable = NULL;
size_t handleTableCapacity = 0;
size_t handleTableSize = 0;

void initializeHandleTable(size_t initialSize) {
    handleTableCapacity = initialSize;
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
}

int addHandle(int socketNum, char * handle) {
    /* Check if handle already in table. */
    for (int i = 0; i < handleTableSize; i++) {
        if (handleTable[i].validFlag && strcmp(handleTable[i].handle, handle) == 0) {
            return -1;
        }
    }

    if (handleTableSize >= handleTableCapacity) {
        resizeHandleTable();
    }

    handleTable[handleTableSize].handle = malloc(strlen(handle) + 1);
    if (handleTable[handleTableSize].handle == NULL) {
        perror("Failed to allocate Handle.");
        freeHandleTable();
        exit(EXIT_FAILURE);
    }
    strcpy(handleTable[handleTableSize].handle, handle);
    handleTable[handleTableSize].validFlag = 1;
    handleTableSize++;
    return 0;
}

void removeHandle(size_t socketNum) {
    if (index < handleTableSize && handleTable[socketNum].validFlag) {
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
    if (index < handleTableSize && handleTable[socketNum].validFlag) {
        return handleTable[socketNum].handle;
    }
    return NULL;
}