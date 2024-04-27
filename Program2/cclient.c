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
#define DEBUG_FLAG 1

int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum, char *clientHandle);
void processMsgFromServer(int socketNum);
void processStdin(int socketNum);
void connectToServer(int socketNum, char *clientHandle);
int packMessage(char* buffer, size_t buffer_size, int flag, int numHandles, char* srcHandle, char* destHandle, char* text);

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

	while (1) {

		int ret = poll(fds, 2, -1);
        if (ret == -1) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            processMsgFromServer(socketNum);
        }

        if (fds[1].revents & POLLIN) {
			printf("Hello\n");
			/* Need to accept commands here. */
            char buf[MAXBUF];
            if (fgets(buf, sizeof(buf), stdin) == NULL) break;

            int len = strlen(buf);
            if (buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
                len--;
            }
			printf("Hell\n");

            char *command = strtok(buf, " ");
            if (command == NULL) {
                printf("Invalid command format.\n");
                continue;
            }
			printf("Hel\n");

			switch (command[1]) {
				case 'M': {
					printf("He\n");
					char buffer[MAXBUF];
					char *destinationHandle = strtok(NULL, " ");
					printf("H\n");
					char *text = strtok(NULL, "");  // Get the rest of the text
					printf("Heh\n");
					// if (destinationHandle == NULL || text == NULL) {
					// 	printf("Invalid format for M command.\n");
					// 	break;
					// }
					printf("Sending message to %s.\n", destinationHandle);
					int result = packMessage(buffer, MAXBUF, 5, 1, clientHandle, destinationHandle, text);
					int sent = sendPDU(socketNum, buffer, result);

					if (sent < 0) {
						perror("Error sending data");
						break; 
					}
					break;
				}
				default: {
					printf("Invalid command.\n");
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

	buffer[offset++] = flag; /* Flag */
	buffer[offset++] = srcHandleLen; /* SrcHandleLen */
	buffer[offset] = srcHandle; /* SrcHandle */
	offset+=srcHandleLen;
	buffer[offset++] = numHandles; /* # of handles. */
	buffer[offset++] = destHandleLen; /* DestHandleLen */
	buffer[offset] = destHandle; /* DestHandle */
	offset+=destHandleLen;
	buffer[offset++] = textLen; /* TextLen */
	buffer[offset] = text; /* Text */
	offset += textLen;
	buffer[offset] = '\0'; /* Null term */

	return totalLength;
	/* Check if text is too big, split text up. */
}

void processMsgFromServer(int socketNum) {
	int recvLen;
	uint8_t recvBuf[MAXBUF];
	memset(recvBuf, 0, sizeof(recvBuf));

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
				printf("Connected to the server.\n$: ");
				fflush(stdout);
				break;
			}
			case 3: {
				/* Bad handle. Quit client. */
				printf("Handle name already taken. Please try another handle name.\n");
				exit(EXIT_FAILURE);
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
