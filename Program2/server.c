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

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"
#include "handleTable.h"

#define MAXBUF 1024
#define MAXHANDLE 100
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);
void serverControl(int mainSocket);
void processClient(int clientSocket);
void extractDestHandle(const uint8_t* buffer, char* destHandle);

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

void addNewSocket(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        perror("accept");
        return;
    }

    printf("New client connected: %d\n", clientSocket);
    addToPollSet(clientSocket);
}

void serverControl(int mainSocket) {
	setupPollSet();
	addToPollSet(mainSocket);

	while(1) {
		int nextSocket;
		nextSocket = pollCall(-1); /* Polls forever until a socket is ready. */
		if (nextSocket == mainSocket) {
			addNewSocket(nextSocket); /* New client trying to connect. */
		}
		else {
			processClient(nextSocket);
		}
	}
}

void processClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		// if (messageLen < MAXBUF) {
        //     dataBuffer[messageLen] = '\0';
        // } else {
        //     dataBuffer[MAXBUF - 1] = '\0';
        // }
		int flag = dataBuffer[0];
		printf("Flag before switch: %d\n", flag);

		switch (flag) {
			case 1: {
				printf("Flag: %d\n", flag); 
				int handleCheck;
				handleCheck = addHandle(clientSocket, dataBuffer);
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
					printf("New Handle: %s\n", dataBuffer);
				}
				break;
			}
			/* Message */
			case 5: {
				printf("Received message.\n");
				/* Need to get dest handle. */
				char destHandle[MAXHANDLE];
				extractDestHandle(dataBuffer, destHandle);
				printf("Dest handle is %s\n", destHandle);
				/* Use dest handle to find socket number. */

				int destSocket = findSocket(destHandle);
				printf("Dest socket is %d\n", destSocket);

				int sent = sendPDU(clientSocket, dataBuffer, MAXBUF);
				break;
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
		close(clientSocket);
		removeFromPollSet(clientSocket);
	}

}

void extractDestHandle(const uint8_t* buffer, char* destHandle) {
    if (buffer == NULL) {
        return;
    }

    int offset = 0;
    uint8_t flag = buffer[offset++];
	printf("Flag is %d\n", flag);
    uint8_t srcHandleLen = buffer[offset++];
	printf("Src is %d\n", srcHandleLen);
    offset += srcHandleLen;
    uint8_t numHandles = buffer[offset++];
	printf("Number of Handles is %d\n", numHandles);
    uint8_t destHandleLen = buffer[offset++];
	printf("Dest is %d\n", destHandleLen);
    memcpy(destHandle, buffer + offset - 1, destHandleLen);
    destHandle[destHandleLen] = '\0'; // Null-terminate the string
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

