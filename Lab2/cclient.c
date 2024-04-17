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

#define MAXBUF 1024
#define DEBUG_FLAG 1

int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int socketNum);
void processMsgFromServer(int socketNum);
void processStdin(int socketNum);
void sendToServer(int socketNum, char * buf);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	addToPollSet(socketNum);
	printf("Enter message: ");
	fflush(stdout);
	addToPollSet(STDIN_FILENO);

	clientControl(socketNum);
	
	close(socketNum);
	
	return 0;
}

void clientControl(int socketNum) {
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
            char buf[MAXBUF];
            if (fgets(buf, sizeof(buf), stdin) == NULL) break;
			int len = strlen(buf) - 1;

			int sent = sendPDU(socketNum, (uint8_t *)buf, len);
			if (sent < 0) {
        		perror("Error sending data");
        		return ; 
    		}
        }

        if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
            printf("\nServer terminated.\n");
            break;
        }
	}
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
		printf("%.*s\n", recvLen, recvBuf);
		printf("Enter message: ");
		fflush(stdout);
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

void sendToServer(int socketNum, char * buf)
{
	uint8_t sendBuf[MAXBUF];
	int sendLen = 0;      
	int sent = 0;          
	
	sendLen = readFromStdin(sendBuf);
	printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);
	
	sent =  sendPDU(socketNum, (uint8_t *)buf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Amount of data sent is: %d\n", sent);
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
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
