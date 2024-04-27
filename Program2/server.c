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
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);
void serverControl(int mainSocket);
void processClient(int clientSocket);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;
	int portNumber = 0;
	
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
		if (messageLen < MAXBUF) {
            dataBuffer[messageLen] = '\0';
        } else {
            dataBuffer[MAXBUF - 1] = '\0';
        }
		printf("Message received on socket %d,", clientSocket);
		printf(" Length: %d,", messageLen);
		printf(" Flag: %d,", dataBuffer[0]); // Displaying the flag byte
		printf(" Message: %s\n", dataBuffer);

    	sendPDU(clientSocket, (uint8_t *)dataBuffer, messageLen);
	}
	else if (messageLen == 0)
	{
        printf("Client %d disconnected.\n", clientSocket);
		close(clientSocket);
		removeFromPollSet(clientSocket);
	}

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

