/***********************************************
*
*Filename: server.c
*
*@author: Xianghang Mi
*@email: mixianghang@outlook.com
*@description: ---
*Create: 2015-11-29 18:03:39
# Last Modified: 2015-12-01 15:57:27
************************************************/
#include <stdio.h> /*printf */
#include <stdint.h>/*uint32_t .etc.*/
#include <stdlib.h>/* atoi*/
#include <sys/socket.h>/* socket, AF_INET SOCK_DGRAM SOCK_STREAM*/
#include <netinet/in.h>/* sockaddr_in INADDR_ANY*/
#include <arpa/inet.h>/* htonl htons ntohl ntohs*/
#include <pthread.h>/*pthread_t pthread_create*/
#include <unistd.h>
#include "server.h"
#include "dv.h"

int main (int argc, char * argv[]) {
  if (argc < 7) {
	printf("Usage : server config portnumber ttl infinity period isSplitHorizon\n");
	printf("ttl should be less than 65536\n");
	printf("infinity should be less than 65536\n");
	printf("period should be less than 65536\n");
	printf("isSplitHorizon is 0 or 1, 0 means to disable split horizon\n");
	return 1;
  }
  char *config  = argv[1];
  int portNum   = atoi(argv[2]);
  int ttl       = atoi(argv[3]);
  int infinity  = atoi(argv[4]);
  int period    = atoi(argv[5]);
  int isSH      = atoi(argv[6]);

  //create socket and bind to port
  int sockFd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in addr_in;
  int addrin_len = sizeof(struct sockaddr_in);
  addr_in.sin_family = AF_INET;
  addr_in.sin_port   = htons(portNum);
  addr_in.sin_addr.s_addr   = INADDR_ANY;

  if (bind(sockFd, (struct sockaddr *) &addr_in, (socklen_t)addrin_len) == 0) {
	printf("bind successfully to port %u \n", portNum);
  } else {
	printf("failed to bind to port %u \n", portNum);
	return 1;
  }

  //test ip convert
  //char ipText[256] = "156.56.83.24";
  //uint8_t ipResult[256] = {0};
  //convertIpText(ipText, ipResult);
  //printf("%u, %u, %u, %u\n", ipResult[0], ipResult[1], ipResult[2], ipResult[3]);

  // read config file and initialize 
  Panel dvPanel;
  dvPanel.port      = portNum;
  dvPanel.sockFd    = sockFd;
  dvPanel.ttl       = ttl;
  dvPanel.infinity  = infinity;
  dvPanel.isSH      = isSH;
  dvPanel.period    = period;
  initPanel(&dvPanel);

  if (initFromConfig(&dvPanel, config) == 0) {
	printf("initialize from config %s successfully\n", config);
	echoProfile(&dvPanel);
  } else {
	printf("failed to initialize from config %s\n", config);
	return 1;
  }

  // set up a new thread for periodically sending out update msg
  pthread_t updateThread;
  pthread_create(&updateThread, NULL, periodUpdate, (void *) &dvPanel);

  //set up a new thread for periodically minus ttl of forward records
  pthread_t ttlThread;
  pthread_create(&ttlThread, NULL, ttlCheck, (void *)&dvPanel);

  // recursively receive update msg and send trigger update msg
  while (1) {

	//recv update msg
	struct sockaddr_in tempAddr;
	int addr_len = sizeof(struct sockaddr_in);
	unsigned char buffer[2046] = {0};
	int recvLen = 0;
	if ((recvLen = recvfrom(sockFd, buffer, sizeof buffer,0,  (struct sockaddr *) &tempAddr, (socklen_t *)&addr_len)) <= 0) {
	  printf("recv update msg failed\n");
	  close(sockFd);
	  return 1;
	}
	//process update msg
	if (processUpdateMsg(&dvPanel, buffer, recvLen, (uint32_t) (tempAddr.sin_addr.s_addr)) != 0) {
	  printf("process update msg error\n");
	  close(sockFd);
	  return 1;
	}

	//trigger update msg sending
	if (triggerUpdateMsg(&dvPanel) != 0) {
	  printf("trigger update msg error\n");
	  close(sockFd);
	  return 1;
	}
  }
}

// convert ip text to binary format
int convertIpText ( char *ipText, uint8_t *result) {
  inet_pton(AF_INET, ipText, result);
  return 0;
}
