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
void terminal();

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	addToPollSet(socketNum);

	/* Connect to server */
	connectToServer(socketNum, argv[3]);

	// printf("Enter message: ");
	// fflush(stdout);
	// addToPollSet(STDIN_FILENO);

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
		else {
			// printf("Sent %d bytes.\n", sent);
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
					// printf("Sending %s to %s\n", text, destinationHandle);
					int result = packMessage(buffer, MAXBUF, 5, 1, clientHandle, destinationHandle, text);
					int sent = sendPDU(socketNum, buffer, result);

					if (sent < 0) {
						perror("Error sending data");
						break; 
					}
					// terminal();
					memset(buffer, 0, MAXBUF);
					break;
				}
				default: {
					printf("Invalid command.\n");
					memset(buf, 0, sizeof(buf)); 
					// terminal();
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

int packMessage(char* buffer, size_t buffer_size, int flag, int numHandles, char* srcHandle, char* destHandle, char* text) {
	if (!buffer || !srcHandle || !destHandle || !text) return -1;

	size_t srcHandleLen = strlen(srcHandle);
    size_t destHandleLen = strlen(destHandle);
    size_t textLen = strlen(text);
    int offset = 0;

	size_t totalLength = 1 + srcHandleLen + 1 + 1 + destHandleLen + 1 + textLen + 1;

    buffer[offset++] = (char)flag;              // Flag
    buffer[offset++] = (char)srcHandleLen;      // SrcHandleLen
    memcpy(buffer + offset, srcHandle, srcHandleLen);  // Copy srcHandle
    offset += srcHandleLen;
    buffer[offset++] = (char)numHandles;        // Number of handles
    buffer[offset++] = (char)destHandleLen;     // DestHandleLen
    memcpy(buffer + offset, destHandle, destHandleLen); // Copy destHandle
    offset += destHandleLen;
    buffer[offset++] = (char)textLen;           // TextLen
    memcpy(buffer + offset, text, textLen);     // Copy text
    offset += textLen;
    buffer[offset] = '\0'; 

	return totalLength;
	/* Check if text is too big, split text up. */
}

void processMsgFromServer(int socketNum) {
	int recvLen;
	uint8_t recvBuf[MAXBUF];
	// memset(recvBuf, 0, sizeof(recvBuf));

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
				// printf("Connected to the server.\n");
				// fflush(stdout);
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
				// terminal();
				// memset(destHandle, 0, MAXHANDLE);
				// memset(message, 0, 200);
				break;
			}
			case 7: {
				/* Destination handle does not exist. */
				break;
			}
			case 9: {
				/* ACKing the client's exit. */
				break;
			}
			case 11: {
				/* Serving sending number of handles stored on server. */
				break;
			}
			case 12: {
				/* A packet of one of the handles from handle list. */
				break;
			}
			case 13: {
				/* Handle list is complete. */
				break;
			}
			default: {
				printf("Invalid flag\n");
				break;
			}
		}
	}

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

	uint8_t destHandle = buffer[offset++];
	uint8_t messageLen = buffer[offset];
    memcpy(message, buffer + offset, messageLen);
    message[messageLen] = '\0';
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