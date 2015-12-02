/***********************************************
*
*Filename: dv.c
*
*@author: Xianghang Mi
*@email: mixianghang@outlook.com
*@description: ---
*Create: 2015-11-29 18:03:31
# Last Modified: 2015-12-01 16:39:11
************************************************/
#include "dv.h"
#include <string.h>
#include <sys/socket.h>/* socket, AF_INET SOCK_DGRAM SOCK_STREAM*/
#include <netinet/in.h>/* sockaddr_in INADDR_ANY*/
#include <arpa/inet.h>/* htonl htons ntohl ntohs*/
#include <pthread.h>/*pthread_t pthread_create*/
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>/**sleep */

/**
*@decription initialize panel.forwardTable panel.nodeNum
*@param panel pointer to Panel Struct
*@param configFile 
*return 0 success, else error
*/
int initFromConfig (Panel *panel, char *configFile) {
  // check file existance
  if (!checkFileExist(configFile)) {
	return 1;
  }
  // open file
  FILE * fd = fopen(configFile, "r");
  if (!fd) {
	return 1;
  }

  char ipText[16];
  char isNeighbor[8];
  memset(ipText, 0, sizeof ipText);
  memset(isNeighbor, 0, sizeof isNeighbor);
  int nodeNum = 0;
  int neighborNum = 0;
  while (fscanf(fd, "%s %s\n", ipText, isNeighbor) == 2) {
	//convert ip str to uint32_t
	uint32_t ipInt;
	inet_pton(AF_INET, ipText, (void *) &ipInt);
	//assign this ip to a record.dest.ip
	Record *tempRecord = &(panel->forwardTable[nodeNum]);
	tempRecord->dest = ipInt;
	tempRecord->ttl  = panel->ttl;
	if (strcmp(isNeighbor, "yes") == 0) {// if neighbor, 
	  tempRecord->nextHop = ipInt;
	  tempRecord->cost    = 1;// neighbor cost is 1
	  panel->neighbor[neighborNum++] = ipInt;
	} else {
	  tempRecord->cost   = panel->infinity;
	  tempRecord->nextHop = 0;
	}
	// if this is a neighbor, add to neighbor array
	nodeNum++;
  }
  panel->nodeNum = nodeNum;
  panel->neighborNum = neighborNum;
  fclose(fd);
  return 0;
}

int main1() {
  Panel panel;
  panel.ttl = 90;
  panel.infinity = 65535;
  char config[256] = "config";
  initFromConfig(&panel, config);
  printf("nodes %d, neighbors %d\n", panel.nodeNum, panel.neighborNum);
  //int i = 0;
  //for(; i< panel.nodeNum; i++) {
  //  Record record = panel.forwardTable[i];
  //  uint32_t dest = record.dest;
  //  printf("%u.%u.%u.%u, cost %d\n", dest & 0xFF, (dest >> 8) & 0xFF, (dest >> 16) & 0xFF, (dest >> 24) & 0xFF, record.cost);
  //}
  return 0;
}

/**
*@description compose update msg for a neighbor 
*@param panel Panel *
*@param result char * store composed msg
*@param maxBytes int how many bytes can be stored in the result array
*@return int length of composed msg, negative means error
*/
int composeUpdateMsg (uint32_t neighbor, Panel *panel, char *result, int maxBytes) {
  uint8_t isSH = panel->isSH;
  int nodeNum = panel->nodeNum;
  if (nodeNum * 8 > maxBytes) {
	printf("need more buffer to store the whole msg\n");
	return -1;
  }
  int i = 0;
  char * temp = result;
  for (; i< nodeNum; i++) {
	Record tempRecord = panel->forwardTable[i];
	uint32_t tempIp = tempRecord.dest;
	*(temp) = (tempIp >> 24) & 0xFF;
	*(temp + 1) = (tempIp >> 16) & 0xFF;
	*(temp + 2) = (tempIp >> 8) & 0xFF;
	*(temp + 3) = tempIp & 0xFF;
	uint32_t cost = tempRecord.cost;
	if (isSH && tempRecord.nextHop == neighbor) {// split horizon
	  cost = panel->infinity;
	}
	*(temp + 4) = (cost >> 24) & 0xFF;
	*(temp + 5) = (cost >> 16) & 0xFF;
	*(temp + 6) = (cost >> 8) & 0xFF;
	*(temp + 7) = cost & 0xFF;
	temp += 8;
  }
  return nodeNum * 8;
}

// periodic update msg
void * periodUpdate (void *panel_1) {
  Panel *panel = (Panel *)panel_1;
  uint16_t period = panel->period;
  while (1) {
	sendUpdateToNeighbors(panel);
	sleep(period);
  }
}

//ttl check
void * ttlCheck(void *panel_1) {
  Panel *panel = (Panel *)panel_1;
  int count = 0;
  while (1) {
	sleep(1);
	int nodeNum = panel->nodeNum;
	int hasUpdate = 0;
	int i = 0;
	for (i = 0; i < nodeNum; i++) {
	  Record *record = &(panel->forwardTable[i]);
	  if (record->ttl != 0) {
		record->ttl -= 1;
		if (record->ttl == 0) {// if expired, set cost to infinity, next hop to zero(no exist)
		  record->cost = panel->infinity;
		  record->nextHop = 0;
		  hasUpdate = 1;
		}
	  }
	}
	if (hasUpdate) {
	  panel->isUpdated = 1;
	}
	count++;
	if (count % 10 == 0) {
	  printf("profile in ttlCheck for every 10 seconds\n");
	  echoProfile(panel);
	  count = 0;
	}
  }
}

//initialize dv panel
void initPanel(Panel * panel) {
  panel->isUpdated = 0;
  panel->nodeNum   = 0;
  panel->neighborNum = 0;
}

//process update msg
int processUpdateMsg(Panel * panel, unsigned char * buffer, int bufferLen, uint32_t fromIp) {
  char fromBuffer[16] = {0};
  convertIp2Text(fromIp, fromBuffer);
  printf("recv update msg from %s, %u with len %d\n", fromBuffer, fromIp, bufferLen);
  // find out cost to neighbor from IP
  uint32_t cost_neighbor = 0;
  int j = 0; 
  for (; j < panel->nodeNum; j++) {
	Record * tempRecord = &(panel->forwardTable[j]);
	if (tempRecord->dest == fromIp && tempRecord->nextHop == fromIp) {
	  cost_neighbor = tempRecord->cost;
	  tempRecord->ttl = panel->ttl;// refresh ttl of this neighbor node
	  break;
	}
  }

  if (cost_neighbor == 0) {
	printf("there must be some error with update msg from %s\n", fromBuffer);
	echoProfile(panel);
	return 1;
  }
  unsigned char * temp = buffer;
  int i = 0;
  while (i * 8 < bufferLen) {

	//convert ip and cost
	uint32_t nodeIp = (*(temp) << 24) | (*(temp + 1) << 16) | (*(temp + 2) << 8) | *(temp + 3);
	uint32_t cost   = (*(temp + 4) << 24) | (*(temp + 5) << 16) | (*(temp + 6) << 8) | *(temp + 7);
	cost += cost_neighbor;

	//check if this cost + 1 is smaller than  current cost to nodeIp
	int j = 0; 
	for (; j < panel->nodeNum; j++) {
	  Record * tempRecord = &(panel->forwardTable[j]);
	  if (tempRecord->dest == nodeIp)  {
		if (tempRecord->cost > cost) {
		  tempRecord->nextHop = fromIp;
		  tempRecord->cost    = cost;
		  panel->isUpdated = 1;
		}
		if (tempRecord->nextHop == fromIp) {
		  tempRecord->ttl     = panel->ttl;//refresh ttl to default value
		}
	  }
	}
	i++;
	temp += 8;
  }
  return 0;
}

//trigger update msg
int triggerUpdateMsg(Panel * panel) {
  if (panel->isUpdated) {
	sendUpdateToNeighbors(panel);
  }
  panel->isUpdated = 0;
  return 0;
}

int checkFileExist(char * filePath) {
	struct stat statStruct;
	return ( stat(filePath,&statStruct) ) == 0;
}


//sendUpdateForAllNeighbors
int sendUpdateToNeighbors(Panel * panel) {
  int neighborNum = panel->neighborNum;
  int i = 0;
  for (; i< neighborNum; i++) {
	//compose msg for each neighbor
	uint32_t neighbor = panel->neighbor[i];
	if (!checkReachability(panel, neighbor)) {
	  printf("some neighbor is broken\n");
	  i++;
	  continue;
	}
	uint32_t msgLen = 0;
	char buffer[4096] = {0};
	msgLen = composeUpdateMsg(neighbor, panel, buffer, 4095);
	printf("composed update msg is %d length\n", msgLen);

	//send msg for each neighbor
	struct sockaddr_in ng_addr;
	int addrLen = sizeof (struct sockaddr_in);
	ng_addr.sin_family = AF_INET;
	ng_addr.sin_port   = htons(panel->port);
	ng_addr.sin_addr.s_addr   = neighbor;
	char ipText[16] = {0};
	convertIp2Text(neighbor, ipText);
	int sentLen = 0;
	int tempLen = 0;
	while (sentLen < msgLen) {
	  tempLen = sendto(panel->sockFd, buffer + sentLen, msgLen - sentLen, 0, (struct sockaddr *) &ng_addr, (socklen_t) addrLen);
	  if (tempLen < 0) {
		printf("sending out update msg failed for neighbor %s\n", ipText); 
		return 1;
	  }
	  sentLen += tempLen;
	}
	printf("finish sending update msg for neighbor %s\n", ipText);
  }
  return 0;
}

//checkReachability
int checkReachability(Panel * panel, uint32_t ip) {
  int i = 0;
  for (; i < panel->nodeNum; i++) {
	Record *tempRecord = &(panel->forwardTable[i]);
	if (tempRecord->dest == ip) {
	  if (tempRecord->cost < panel->infinity) {
		return 1;// is reachable
	  } else {
		return 0;
	  }
	}
  }
  return 0;
}

//convert ipInt to text
int convertIp2Text (uint32_t ip, char * buffer) {
  sprintf(buffer, "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
  return 0;
}

//echo profile of Panel
int echoProfile(Panel * panel) {
  printf("current DV profile\n");
  printf("nodes: %d, neighbors: %d, infinity: %d, ttl: %d, period: %d\n", panel->nodeNum, panel->neighborNum, panel->infinity, panel->ttl, panel->period);
  int i = 0;
  for (;i < panel->nodeNum; i++) {
	Record *tempRecord = &(panel->forwardTable[i]);
	char destBuffer[16] = {0};
	char nextHopBuffer[16] = {0};
	convertIp2Text(tempRecord->dest, destBuffer);
	convertIp2Text(tempRecord->nextHop, nextHopBuffer);
	printf("node: %s, %u, nextHop : %s, %u, cost: %u, ttl: %u\n", destBuffer, tempRecord->dest, nextHopBuffer,tempRecord->nextHop, tempRecord->cost, tempRecord->ttl);
  }
  return 0;
}
