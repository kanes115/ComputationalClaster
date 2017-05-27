#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>

#include "Definitions.h"


//Globals
char name[CLIENTS_MAX_NAMELEN];
int communicationWay;
char address[MAX_PATH_LEN];
int port;
int serv_fd;

char* calcResult;
//***



//Parse
void showUsageClose(){
  fprintf(stderr, "%s\n", "Usage:\n./Client <name> <local|net> <ipv4|path>");
  exit(-1);
}

void parse(int argc, char* argv[], char* name, int* communicationWay, char* address, int* port){
  if(argc < 4)
    showUsageClose();

  strcpy(name, argv[1]);

  if(strcmp(argv[2], "local") == 0)
    *communicationWay = AF_LOCAL;
  else if(strcmp(argv[2], "ipv4") == 0)
    *communicationWay = AF_INET;
  else
    showUsageClose();

  strcpy(address, argv[3]);

  if(*communicationWay == AF_INET)
    *port = atoi(argv[4]);
  else
    *port = -1;
}
//***

//Preparing socket
int prepareSocket(int sockType){
  struct sockaddr_in serv_addr;
  int serv_fd;

  struct in_addr serv_addr;
  inet_aton(address, &serv_addr);

  serv_fd = socket(sockType, SOCK_STREAM, 0);
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = sockType;
  serv_addr.sin_addr.s_addr = serv_addr;
  if(communicationWay == AF_INET)
    serv_addr.sin_port = htons(port);

  bind(serv_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));

  return serv_fd;
}
//***


void sendPingMsg(){
  char* buf = malloc(1);
  buf[0] = 0;
  send(serv_fd, buf, 1, 0);
}

void sendRegisterMsg(){
  char* buf = malloc(MAX_MSG_LEN);
  sprintf(buf, "reg:%s", name);
  send(serv_fd, buf, strlen(buf) + 1, 0);
}

void sendResultMsg(char* result){
  char* buf = malloc(MAX_MSG_LEN);
  sprintf(buf, "res:%s", result);
  send(serv_fd, buf, strlen(buf) + 1, 0);
}

//***




void run(){

  char buf[MAX_MSG_LEN];

  while(1){
    if(recv(serv_fd, buf, MAX_MSG_LEN, 0)){
      serveDataMsg(buf);
    }
  }
}





int main(int argc, char* argv[]){
  parse(argc, argv, name, &communicationWay, address, port);
  serv_fd = prepareSocket(communicationWay);

  sendTextMsg(REGISTER);


  run();
}
