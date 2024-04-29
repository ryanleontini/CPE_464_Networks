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
int sendErrorPacket(uint8_t* errorBuf, char* destHandle, int destHandleLen);
int getMessageOffset(uint8_t* dataBuffer);
int sendHandleNum(int clientSocket);
void sendFinish(int clientSocket);
void sendFlag (int socket, int flag);
int packMessage(char* buffer, size_t buffer_size, int flag, int numHandles, char* srcHandle, char* destHandle, char* text);
void unpackMulticast(int clientSocket, char* buffer);

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
				}
				break;
			}
			/* Message */
			case 5: {
				printf("Message received.\n");
				char destHandle[MAXHANDLE];
				char errorBuf[MAXBUF];
				int destHandleLen = extractDestHandle(dataBuffer, destHandle);
				int destSocket = findSocket(destHandle);
				int offset = sendErrorPacket(errorBuf, destHandle, destHandleLen);

				if (destSocket == -1) {
					printf("Invalid handle supplied.\n");
					int sent = sendPDU(clientSocket, errorBuf, offset+2);
					memset(errorBuf, 0, MAXBUF);
					break;
				} else {
					/* Check message size. */
					int messageOffset = getMessageOffset(dataBuffer);
					char *message = dataBuffer + messageOffset;
					int messageLent = strlen(message) + 1; 
					int sent;
					if (messageLent > 200) {
						int bytesToSend = messageLent;
						int offset = 0;

						while (bytesToSend > 0) {
							int chunkSize = (bytesToSend > 199) ? 199 : bytesToSend;
							char packetBuffer[200]; 
							memcpy(packetBuffer, message + offset, chunkSize);
							packetBuffer[chunkSize] = '\0'; 

							sent = sendPDU(destSocket, packetBuffer, chunkSize + 1); 
							bytesToSend -= chunkSize;
							offset += chunkSize;
						}
					} else {
						sent = sendPDU(destSocket, dataBuffer, messageLen);
					}
					fflush(stdout);
					break;
				}
			}
			case 6: {
				/* Multicast */
				printf("Received multicast.\n");
				unpackMulticast(clientSocket, dataBuffer);
				break;
			}
			case 8: {
				sendFlag(clientSocket, 9);
				removeFromPollSet(clientSocket);

				printf("Client %d disconnected.\n", clientSocket);
				removeHandle(clientSocket);
				close(clientSocket);
				break;
			}
			case 10: {
				int numHandles = sendHandleNum(clientSocket);
				/* Send handles */
				int size = getHandleTableSize();

				for (int i = 0; i < size; i++) {
					Handle *temp = NULL;
					temp = getHandleAtIndex(i);
					if (temp->handle != NULL && temp->validFlag == 1) {
						char handleBuf[120];
						handleBuf[0] = 12;
						int handleLength = strlen(temp->handle);
						handleBuf[1] = handleLength;
						memcpy(handleBuf + 2, temp->handle, handleLength);
						int sent = sendPDU(clientSocket, handleBuf, handleLength + 2);
					}
				}

				sendFinish(clientSocket);
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

void unpackMulticast(int clientSocket, char* buffer) {
	int bufferIndex = 1;
	int srcHandleLen = buffer[bufferIndex++];
	char srcHandle[100];
	memcpy(srcHandle, buffer + bufferIndex, srcHandleLen);
    srcHandle[srcHandleLen] = '\0'; 
	bufferIndex += srcHandleLen;
    int numHandles = buffer[bufferIndex++];

	char tempDestBuff[500];
	int tempDestCount = 0;
    for (int i = 0; i < numHandles; i++) {
		int destHandleLen = buffer[bufferIndex++];
		tempDestBuff[tempDestCount++] = destHandleLen;
		char destHandle[100];
		memcpy(tempDestBuff + tempDestCount, buffer + bufferIndex, destHandleLen);
		memcpy(destHandle, buffer + bufferIndex, destHandleLen);
    	destHandle[destHandleLen] = '\0'; 
		tempDestCount += destHandleLen;
		bufferIndex += destHandleLen;
    }
	char *currentPointer = buffer + bufferIndex;
    if (*currentPointer == '\0') {
        printf("No message found in packet\n");
        return;
    }

	int tempCount = 0;
	char currentHand[100];
    for (int i = 0; i < numHandles; i++) {
		char newBuf[MAXBUF];
		int currentLen = tempDestBuff[tempCount++];
		memcpy(currentHand, tempDestBuff + tempCount, currentLen);
    	currentHand[currentLen] = '\0'; 
		tempCount += currentLen;

		int result = packMessage(newBuf, MAXBUF, 5, 1, srcHandle, currentHand, currentPointer);
		int socket = findSocket(currentHand);
		int sent = sendPDU(socket, newBuf, result);
		memset(newBuf, 0, MAXBUF);
		memset(currentHand, 0, 100);
	}
}

int packMessage(char* buffer, size_t buffer_size, int flag, int numHandles, char* srcHandle, char* destHandle, char* text) {
	if (!buffer || !srcHandle || !destHandle || !text) return -1;

	size_t srcHandleLen = strlen(srcHandle);
    size_t destHandleLen = strlen(destHandle);
    size_t textLen = strlen(text);
    int offset = 0;

	size_t totalLength = 1 + srcHandleLen + 1 + 1 + destHandleLen + 1 + textLen + 1;

    buffer[offset++] = (char)flag;              
    buffer[offset++] = (char)srcHandleLen;      
    memcpy(buffer + offset, srcHandle, srcHandleLen);  
    offset += srcHandleLen;
    buffer[offset++] = (char)numHandles;       
    buffer[offset++] = (char)destHandleLen;    
    memcpy(buffer + offset, destHandle, destHandleLen);
    offset += destHandleLen;
    memcpy(buffer + offset, text, textLen);  
    offset += textLen;
    buffer[offset] = '\0'; 

	return totalLength;
}

void sendFlag (int socket, int flag) {
	char flagBuf[1];
	flagBuf[0] = flag;
	int sent = sendPDU(socket, flagBuf, 1);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
}

void sendFinish(int clientSocket) {
	char buf[1];
	buf[0] = 13;
	int sent = sendPDU(clientSocket, buf, 1);
}

int sendHandleNum(int clientSocket) {
	int32_t validHandles = getValids();
	printf("There are %d handles in the table.\n", validHandles);
	char handleBuf[5];
	handleBuf[0] = 11;
	int32_t netForm = htons(validHandles);
	memcpy(handleBuf + 1, &netForm, 4);
	int sent = sendPDU(clientSocket, handleBuf, 5);
	memset(handleBuf, 0, 5);
	return validHandles;
}

int getMessageOffset(uint8_t* dataBuffer) {
    if (!dataBuffer) return -1; 

    int offset = 0;
    offset += 1;
    uint8_t srcHandleLen = dataBuffer[offset];
    offset += 1 + srcHandleLen; 
    offset += 1;
    uint8_t destHandleLen = dataBuffer[offset];
    offset += 1 + destHandleLen; 
    return offset + 1;
}

int sendErrorPacket(uint8_t* errorBuf, char* destHandle, int destHandleLen) {
	int offset = 0;
	errorBuf[offset++] = 7;
	errorBuf[offset++] = destHandleLen;
	memcpy(errorBuf + offset, destHandle, destHandleLen);
	offset+=destHandleLen;
	errorBuf[offset] = '\0'; 
	return offset;
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

