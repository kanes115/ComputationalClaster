#define  _XOPEN_SOURCE 600
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>



#include "Definitions.h"


//Globals
char name[CLIENTS_MAX_NAMELEN];
int communicationWay;
char char_address[MAX_PATH_LEN];
char port[7];
int serv_fd;

char* calcResult;
//***



//Parse
void showUsageClose(){
  fprintf(stderr, "%s\n", "Usage:\n./Client <name> <local|net> <ipv4|path>");
  exit(-1);
}

void parse(int argc, char* argv[], char* name, int* communicationWay, char* address, char* port){
  if(argc < 4)
    showUsageClose();

  strcpy(name, argv[1]);

  if(strcmp(argv[2], "local") == 0)
    *communicationWay = AF_LOCAL;
  else if(strcmp(argv[2], "net") == 0)
    *communicationWay = AF_INET;
  else
    showUsageClose();

  strcpy(address, argv[3]);

  if(*communicationWay == AF_INET)
    port = argv[4];
  else
    port = "-1";
}

int streq(char *string1, char *string2) {
    return strcmp(string1, string2) == 0;
}

//***

//Preparing socket
int prepareSocket(int sockType){

  int server_socket;
    if (communicationWay == AF_INET) {
        struct addrinfo address, *res;
        memset(&address, 0, sizeof(address));
        address.ai_family = AF_UNSPEC;    //INET?
        address.ai_socktype = SOCK_STREAM;
        getaddrinfo(char_address, port, &address, &res);
        if ((server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
            perror("Socket");
            return -1;
        }
        if (connect(server_socket, res->ai_addr, res->ai_addrlen) < 0) {
            close(server_socket);
            perror("Connection");
            return -1;
        }
    } else if (communicationWay == AF_LOCAL) {
        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, char_address);
        if ((server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("Socket");
            return -1;
        }
        if (connect(server_socket, (const struct sockaddr *) &address, sizeof(address)) < 0) {
            close(server_socket);
            perror("Connection");
            return -1;
        }
    } else {
        server_socket = -1;
    }
    return server_socket;
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
  if(send(serv_fd, buf, strlen(buf) + 1, 0) == -1){
    perror("send");
    exit(1);
  }

  char* resp = malloc(MAX_MSG_LEN);

  while(1){
    printf("ss\n");
    if(recv(serv_fd, resp, MAX_MSG_LEN, 0)){
      if(strcmp(resp, "r:1") == 0){
        printf("This name is taken\n");
        exit(1);
      }else
        return;
    }
  }
}

void sendResultMsg(char* result){
  char* buf = malloc(MAX_MSG_LEN);
  sprintf(buf, "res:%s", result);
  send(serv_fd, buf, strlen(buf) + 1, 0);
}

//***





int main(int argc, char* argv[]){
  parse(argc, argv, name, &communicationWay, char_address, port);
  serv_fd = prepareSocket(communicationWay);

  sendRegisterMsg();
}
