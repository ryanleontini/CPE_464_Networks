#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h> 
#include <sys/socket.h>
#include <stdlib.h>

int sendPDU(int socketNumber, uint8_t * dataBuffer, int lengthOfData) {
    int totalLength;
    int bytesSent;
    uint8_t *pdu;
    uint16_t netLength;

    totalLength = 2 + lengthOfData;

    pdu = malloc(totalLength);
    if (pdu == NULL) {
        perror("Failed to allocate memory for PU");
        return -1;
    }

    netLength = htons(totalLength);
    
    memcpy(pdu, &netLength, 2); /* Copy length into buffer. */
    memcpy(pdu + 2, dataBuffer, lengthOfData); /* Copy data into buffer. */

    bytesSent = send(socketNumber, pdu, totalLength, 0);

    if (bytesSent < 0) {
        perror("Failed to send PDU");
    } else {
        bytesSent -= 2; /* Remove length bytes */
    }

    free(pdu);

    return bytesSent;
}

int recvPDU(int clientSocket, uint8_t * dataBuffer, int bufferSize) {
    uint16_t pduLength;
    int len;
    
    /* First recv for PDU Length */
    len = recv(clientSocket, &pduLength, 2, MSG_WAITALL);
    if (len < 0) {
        perror("Error receiving PDU length");
        return -1;
    } else if (len == 0) { /* Connection closed. */
        return 0; 
    }

    pduLength = ntohs(pduLength) - 2;

    if (pduLength > bufferSize) {
        fprintf(stderr, "Buffer size is too small for the received PDU\n");
        exit(EXIT_FAILURE);
    }

    /* Second recv for PDU data */
    len = recv(clientSocket, dataBuffer, pduLength, MSG_WAITALL);
    if (len < 0) {
        perror("Error receiving PDU data");
        return -1;
    } else if (len == 0) { /* Close connection if zero. */
        return 0;
    }

    return len;
}