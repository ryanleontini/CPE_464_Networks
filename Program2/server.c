/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include <ctype.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"
#include "handleTable.h"

#define MAXBUF 1400
#define MAXHANDLE 100
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);
void serverControl(int mainSocket);
void processClient(int clientSocket);
int extractDestHandle(const uint8_t* buffer, char* destHandle);
int addNewSocket(int serverSocket);

void printDataBuffer(void* buffer, size_t length) {
    uint8_t* byteBuffer = (uint8_t*) buffer;
    printf("Data Buffer Contents:\n");
    for (size_t i = 0; i < length; i++) {
        printf("%02x ", byteBuffer[i]);  // Print hexadecimal value
        if (isprint(byteBuffer[i])) {
            printf("(%c) ", byteBuffer[i]);  // Print ASCII character if printable
        } else {
            printf("(.) ");  // Use a placeholder for non-printable characters
        }
        if ((i + 1) % 8 == 0) {
            printf("\n");  // Print a new line every 8 characters for better readability
        }
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
	int mainServerSocket = 0;
	int portNumber = 0;

	initializeHandleTable(10);
	
	portNumber = checkArgs(argc, argv);
	
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket);

	close(mainServerSocket);

	return 0;
}

int addNewSocket(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        perror("accept");
        return -1;
    }

    printf("New client socket added: %d\n", clientSocket);
    addToPollSet(clientSocket);
	return clientSocket;
}

void serverControl(int mainSocket) {
	int clientSocket;
	setupPollSet();
	addToPollSet(mainSocket);

	while(1) {
		int nextSocket;
		nextSocket = pollCall(-1); /* Polls forever until a socket is ready. */
		if (nextSocket == mainSocket) {
			clientSocket = addNewSocket(nextSocket); /* New client trying to connect. */
		}
		else {
			clientSocket = nextSocket;
			processClient(clientSocket);
		}
	}
}

void processClient(int clientSocket) {
	printf("Processing client %d\n", clientSocket);
	uint8_t dataBuffer[MAXBUF];
	memset(dataBuffer, 0, MAXBUF);
	int messageLen = 0;
	
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	int totalLength = messageLen + 2;

	if (messageLen > 0)
	{
		int flag = dataBuffer[0];

		switch (flag) {
			case 1: {

				int handleCheck;
				handleCheck = addHandle(clientSocket, dataBuffer+1);
				printf("New connection from: %s\n", dataBuffer+1);

				if (handleCheck == -1) {
					printf("Handle name already exists.\n");
					uint8_t newBuf[1];
					newBuf[0] = 3; /* Flag to send to client. */
					sendPDU(clientSocket, (uint8_t *)newBuf, 1);
				}
				else {
					uint8_t newBuf[1];
					newBuf[0] = 2; /* Flag to send to client. */
					sendPDU(clientSocket, (uint8_t *)newBuf, 1);
					// printf("New Handle: %s\n", dataBuffer);
					printHandleTable();
				}
				// memset(dataBuffer, 0, MAXBUF);
				break;
			}
			/* Message */
			case 5: {
				printf("Message received.\n");
				/* Need to get dest handle. */
				char destHandle[MAXHANDLE];
				// memset(destHandle, 0, MAXHANDLE);
				int destHandleLen = extractDestHandle(dataBuffer, destHandle);
				/* Use dest handle to find socket number. */
				// printf("Dest handle: %s\n",destHandle);
				int destSocket = findSocket(destHandle);
				// printHandleTable();
				printf("Dest socket: %d\n", destSocket);
				if (destSocket == -1) {
					printf("Invalid handle supplied.\n");
					char errorBuf[MAXBUF];
					int offset = 0;
					errorBuf[offset++] = 7;
					errorBuf[offset++] = destHandleLen;
    				memcpy(errorBuf + offset, destHandle, destHandleLen);
					offset+=destHandleLen;
					errorBuf[offset] = '\0'; 
					int sent = sendPDU(clientSocket, errorBuf, offset+2);
					break;
				} else {
					int sent = sendPDU(destSocket, dataBuffer, messageLen);
					printf("Bytes sent: %d\n", sent);
					fflush(stdout);
					break;
				}
			}
			default: {
				printf("Invalid flag\n");
				printf("Flag: %d\n", flag); 
				break;
			}
		}
	}
	else if (messageLen == 0)
	{
        printf("Client %d disconnected.\n", clientSocket);
		removeHandle(clientSocket);
		printHandleTable();
		close(clientSocket);
		removeFromPollSet(clientSocket);
	}

}

int extractDestHandle(const uint8_t* buffer, char* destHandle) {
    if (buffer == NULL) {
        return -1;
    }

    int offset = 0;
    uint8_t flag = buffer[offset++];
    uint8_t srcHandleLen = buffer[offset++];
    offset += srcHandleLen;
    uint8_t numHandles = buffer[offset++];
    uint8_t destHandleLen = buffer[offset++];
    memcpy(destHandle, buffer + offset, destHandleLen);
    destHandle[destHandleLen] = '\0';
	return destHandleLen;
}

int checkArgs(int argc, char *argv[])
{
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

