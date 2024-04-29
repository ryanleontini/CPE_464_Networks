/******************************************************************************
* myClient.c
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
#include <errno.h>
#include <poll.h>

#include <ctype.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "pollLib.h"

#define MAXBUF 1400
#define MAXHANDLE 100
#define DEBUG_FLAG 1

int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum, char *clientHandle);
void processMsgFromServer(int socketNum);
void processStdin(int socketNum);
void connectToServer(int socketNum, char *clientHandle);
int packMessage(char* buffer, size_t buffer_size, int flag, int numHandles, char* srcHandle, char* destHandle, char* text);
int extractSrcHandle(const uint8_t* buffer, char* destHandle);
void extractMessage(const uint8_t* buffer, char* message, int offset);
int extractErrorHandle(const uint8_t* buffer, char* destHandle);
void terminal();
void sendFlag (int socket, int flag);
int processMulticast(int socketNum, char* clientHandle);

int main(int argc, char * argv[])
{
	int socketNum = 0;  
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	addToPollSet(socketNum);

	/* Connect to server */
	connectToServer(socketNum, argv[3]);

	clientControl(socketNum, argv[3]);
	
	close(socketNum);
	
	return 0;
}

void connectToServer(int socketNum, char *clientHandle) {
	uint8_t dataBuffer[200];
	int sent;

	/* Max handle size is 100 */
    if (strlen(clientHandle) > 100) {
        printf("Client handle must be less than 100 characters.\n");
    } else {
		dataBuffer[0] = 1; /* Initial connection flag */
        memcpy(&dataBuffer[1], clientHandle, strlen(clientHandle) + 1);
		
		size_t dataSize = strlen(clientHandle) + 1; /* +1 for flag, +1 for null terminator. */

		sent = sendPDU(socketNum, dataBuffer, dataSize);
		if (sent < 0) {
			printf("Failed to send\n");
		}
    }
}

void clientControl(int socketNum, char *clientHandle) {
    struct pollfd fds[2];
    fds[0].fd = socketNum;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;	
	int terminalFlag = 0;

	while (1) {
		if (terminalFlag > 0){ terminal(); }
		int ret = poll(fds, 2, -1);
        if (ret == -1) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            processMsgFromServer(socketNum);
			terminalFlag++;
        }

        if (fds[1].revents & POLLIN) {
			/* Need to accept commands here. */
            char buf[MAXBUF];
            if (fgets(buf, sizeof(buf), stdin) == NULL) break;

            int len = strlen(buf);
            if (buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
                len--;
            }

            char *command = strtok(buf, " ");
            if (command == NULL) {
                printf("Invalid command format.\n");
				memset(buf, 0, sizeof(buf)); 
                continue;
            }

			switch (command[1]) {
				case 'm':
				case 'M': {
					char buffer[MAXBUF];
					char *destinationHandle = strtok(NULL, " ");
					if (destinationHandle == NULL) {
						printf("Destination handle is missing. Please try again.\n");
						continue;
					}
					char *text = strtok(NULL, "");
					if (text == NULL) {
						printf("Text is missing or invalid.\n");
						text = "";
					}
					int result = packMessage(buffer, MAXBUF, 5, 1, clientHandle, destinationHandle, text);
					int sent = sendPDU(socketNum, buffer, result);

					if (sent < 0) {
						perror("Error sending data");
						break; 
					}
					memset(buffer, 0, MAXBUF);
					break;
				}
				case 'c':
				case 'C': {
					int multi = processMulticast(socketNum, clientHandle);
					if (multi == -1) {
						printf("Failed to multicast.\n");
					}
					break;
				}
				case 'b':
				case 'B': {
					break;
				}
				case 'l':
				case 'L': {
					char* buffer[1];
					buffer[0] = 10;
					int sent = sendPDU(socketNum, buffer, 1);
					if (sent < 0) {
						perror("Error sending data");
						break; 
					}
					break;
				}
				case 'e':
				case 'E': {
					sendFlag(socketNum, 8);
					break;
				}
				default: {
					printf("Invalid command.\n");
					memset(buf, 0, sizeof(buf)); 
					break;
				}

			}
        }

        if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
            printf("\nServer terminated.\n");
            break;
        }
	}
}

int processMulticast(int socketNum, char* clientHandle) {
	char buffer[MAXBUF];
	buffer[0] = 6;
	int srcHandleLen = strlen(clientHandle);
	buffer[1] = srcHandleLen;
	memcpy(buffer + 2, clientHandle, srcHandleLen);
	int bufferIndex = 2 + srcHandleLen;

	char *token = strtok(NULL, " "); 
	if (token == NULL) {
		printf("Error parsing number of handles.\n");
		return -1;
	}
	int numHandles = atoi(token);
	printf("Num handles: %d\n", numHandles);
	if (numHandles < 2 || numHandles > 9) { 
		printf("Invalid input. 2-9 handles required.\n");
		return -1;
	}
	buffer[bufferIndex++] = numHandles;

	for (int i = 0; i < numHandles; i++) {
		char *destinationHandle = strtok(NULL, " ");
		if (destinationHandle == NULL) {
			printf("Error parsing handle %d\n", i + 1);
			return -1;
		}

		int destLen = strlen(destinationHandle);
		printf("Destination handle: %s\n", destinationHandle);
		buffer[bufferIndex++] = destLen;
		memcpy(buffer + bufferIndex, destinationHandle, destLen);
		bufferIndex += destLen;
	}
	char *message = strtok(NULL, ""); 
	if (message != NULL) {
		int messageLen = strlen(message);
		printf("Message: %s\n", message);
		memcpy(buffer + bufferIndex, message, messageLen);
		bufferIndex += messageLen;
	}

	if (bufferIndex < MAXBUF) {
		buffer[bufferIndex] = '\0';
	} else {
		buffer[MAXBUF - 1] = '\0';
	}
	int sent = sendPDU(socketNum, buffer, MAXBUF);
	return sent;
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
	/* Check if text is too big, split text up. */
}

void printDataBuffer(void* buffer, size_t length) {
    uint8_t* byteBuffer = (uint8_t*) buffer;
    printf("Data Buffer Contents:\n");
    for (size_t i = 0; i < length; i++) {
        printf("%02x ", byteBuffer[i]); 
        if (isprint(byteBuffer[i])) {
            printf("(%c) ", byteBuffer[i]);  
        } else {
            printf("(.) ");  
        }
        if ((i + 1) % 8 == 0) {
            printf("\n"); 
        }
    }
    printf("\n");
}

void processMsgFromServer(int socketNum) {
	int recvLen;
	uint8_t recvBuf[MAXBUF];

	/* Check if the server is still connected */

	recvLen = recvPDU(socketNum, recvBuf, sizeof(recvBuf) - 1);

	if (recvLen < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			/* No data available right now */
		} else if (errno == ECONNRESET) {
			printf("Server closed. Unable to send message.\n");
			return;
		} else {
			perror("Recv call");
			return;
		}
	} else if (recvLen == 0) {
		return;
	} else {
		int flag = recvBuf[0];
		switch (flag) {
			case 2: {
				/* Connected to the server. */
				break;
			}
			case 3: {
				/* Bad handle. Quit client. */
				printf("Handle name already taken. Please try another handle name.\n");
				exit(EXIT_FAILURE);
				break;
			}
			case 5: {
				char destHandle[MAXHANDLE];
				char message[200];
				/* Need to check message length? */
				int offset;
				offset = extractSrcHandle(recvBuf, destHandle);
				extractMessage(recvBuf, message, offset);
				printf("%s: %s\n", destHandle, message);
				memset(destHandle, 0, MAXHANDLE);
				memset(message, 0, 200);
				break;
			}
			case 7: {
				/* Destination handle does not exist. */
				char destHandle[MAXHANDLE];
				int offset = extractErrorHandle(recvBuf,destHandle);
				printf("Handle '%s' does not exist.\n", destHandle);
				memset(destHandle, 0, MAXHANDLE);
				break;
			}
			case 9: {
				/* ACKing the client's exit. */
				exit(-1);
				break;
			}
			case 11: {
				/* Serving sending number of handles stored on server. */
				int32_t numHandles;
				memcpy(&numHandles, recvBuf + 1, 4);
				numHandles = ntohs(numHandles);
				printf("There's %d handle(s) in the table.\n", numHandles);
				break;
			}
			case 12: {
				/* A packet of one of the handles from handle list. */
				char handle[101];
				int handleLength = recvBuf[1];
				memcpy(handle, recvBuf + 2, handleLength);
				handle[handleLength] = '\0';  
				printf("%s\n", handle);
				break;
			}
			case 13: {
				/* Handle list is complete. */
				printf("Handle table transmission complete.\n");
				break;
			}
			default: {
				printf("Invalid flag\n");
				break;
			}
		}
	}

}

int extractErrorHandle(const uint8_t* buffer, char* destHandle) {
    if (buffer == NULL) {
        return -1;
    }

    int offset = 0;
    uint8_t flag = buffer[offset++];
	printf("Flag is %d\n", flag);
    uint8_t destHandleLen = buffer[offset++];
	printf("desthandlelen is %d\n", destHandleLen);
	memcpy(destHandle, buffer + offset, destHandleLen);
	offset += destHandleLen;

	return offset;
}

int extractSrcHandle(const uint8_t* buffer, char* srcHandle) {
    if (buffer == NULL) {
        return -1;
    }

    int offset = 0;
    uint8_t flag = buffer[offset++];
    uint8_t srcHandleLen = buffer[offset++];
	memcpy(srcHandle, buffer + offset, srcHandleLen);
    srcHandle[srcHandleLen] = '\0';
    offset += srcHandleLen;
    uint8_t numHandles = buffer[offset++];
    uint8_t destHandleLen = buffer[offset++];
	offset += destHandleLen;
	return offset;
}

void extractMessage(const uint8_t* buffer, char* message, int offset) {
    if (buffer == NULL) {
        return;
    }
	int messageLen = strlen((char*)buffer + offset);
    memcpy(message, buffer + offset, messageLen);
}


void processStdin(int socketNum) {
	uint8_t sendBuf[MAXBUF]; 
	int sendLen = 0;      
	int sent = 0;
	
	sendLen = readFromStdin(sendBuf);
	if (fgets((char *)sendBuf, sizeof(sendBuf), stdin) != NULL) {
		sendPDU(socketNum, sendBuf, sendLen);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}

		printf("Amount of data sent is: %d\n", sent);
	}
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s host-name port-number handle \n", argv[0]);
		exit(1);
	}
}

void terminal() {
	printf("$: ");
	fflush(stdout);
}