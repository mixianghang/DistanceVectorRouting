/***********************************************
*
*Filename: dv.h
*
*@author: Xianghang Mi
*@email: mixianghang@outlook.com
*@description: ---
*Create: 2015-11-29 18:03:21
# Last Modified: 2015-12-01 20:01:16
************************************************/

#ifndef DV_H
#define DV_H
#include <stdint.h>
typedef struct RouteNode {
  uint32_t ip;
} Node;

typedef struct ForwardRecord {
  uint32_t dest;// destination
  uint32_t nextHop;// next hop
  uint16_t ttl;
  uint32_t cost;
} Record;


typedef struct DvPanel {
  uint16_t port;
  uint16_t ttl;
  uint16_t period;
  uint8_t isSH;//whether enabling split horizon
  uint8_t isUpdated;//used by trigger to decide whether to send update msg to neighbors
  uint8_t nodeNum;
  uint8_t neighborNum;
  uint16_t infinity;
  int     sockFd; // udp sock fd
  Record forwardTable[256];
  uint32_t   neighbor[256];
} Panel;

//initialize dvPanel
void initPanel(Panel * panel);
/**
*@decription initialize panel.forwardTable panel.nodeNum
*@param panel pointer to Panel Struct
*@param configFile 
*return 0 success, else error
*/
int initFromConfig (Panel *panel, char *configFile);

/**
*@description compose update msg for a neighbor 
*@param neighbor uint32_t
*@param panel Panel *
*@param result char * store composed msg
*@param maxBytes int how many bytes can be stored in the result array
*@return int length of composed msg, negative means error
*/
int composeUpdateMsg (uint32_t neighbor, Panel *panel, char *result, int maxBytes);

// periodic update msg
void * periodUpdate (void *panel);

//ttl check
void * ttlCheck(void *panel);

//process update msg
int processUpdateMsg(Panel * panel, unsigned char * buffer, int bufferLen, uint32_t fromIp);

int checkFileExist(char * filePath);


//sendUpdateForAllNeighbors
int sendUpdateToNeighbors(Panel * panel);

//convert ipInt to text
int convertIp2Text (uint32_t ip, char * buffer);

//trigger update msg
int triggerUpdateMsg(Panel * panel);

//checkReachability
int checkReachability(Panel * panel, uint32_t ip);

//echo profile of Panel
int echoProfile(Panel * panel);



#endif

