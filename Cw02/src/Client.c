#define  _XOPEN_SOURCE 600
#define _BSD_SOURCE
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
#include <signal.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
  fprintf(stderr, "%s\n", "Usage:\n./Client <name> <local|net> <ipv4|path> <port (if net)>");
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

  if(argc != 4 && *communicationWay == AF_LOCAL)
    showUsageClose();
  if(argc != 5 && *communicationWay == AF_INET)
    showUsageClose();

  strcpy(address, argv[3]);

  if(*communicationWay == AF_INET)
    strcpy(port, argv[4]);
  else
    port = "-1";
}

int streq(char *string1, char *string2) {
    return strcmp(string1, string2) == 0;
}

int numbers_only(const char *s){
    while (*s) {
        if (isdigit(*s++) == 0) return 0;
    }
    return 1;
}
//***


int send_msg(int socket, int type, int orderNo, char* expr) {
    struct Message msg;
    msg.type = htonl(type);
    msg.orderNo = htonl(orderNo);
    if(expr != NULL)
      strcpy(msg.expr, expr);
    write(socket, &msg, sizeof msg);
    return 0;
}

void decode_msg(struct Message orig, struct Message* out){
  out->type = ntohl(orig.type);
  out->orderNo = ntohl(orig.orderNo);
  strcpy(out->expr, orig.expr);
}



//Preparing socket
int prepareSocket(int sockType){
  int server_socket;
  printf("port: %d\n", atoi(port));

    if (communicationWay == AF_INET) {
      server_socket = socket(AF_INET, SOCK_DGRAM, 0);
      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      if(!inet_aton(char_address, &addr.sin_addr)){
        perror("Error, wrong IP");
        exit(-1);
      }
      addr.sin_port = atoi(port);
      if(connect(server_socket, (struct sockaddr *)&addr, sizeof(addr))){
        perror("Error");
        exit(-1);
      }
    } else if (communicationWay == AF_LOCAL) {
      server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
      struct sockaddr_un addr;
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, char_address);
      bind(server_socket, (struct sockaddr *)&addr, sizeof(sa_family_t));
      if(connect(server_socket, (struct sockaddr *)&addr, sizeof(addr))){
        perror("Error");
        exit(-1);
      }
    } else {
        server_socket = -1;
    }
    return server_socket;
}
//******************

//Registers client in server
void sendRegisterMsg(){
  char* buf = malloc(MAX_MSG_LEN);
  sprintf(buf, "%s", name);
  if(send_msg(serv_fd, REGISTER, -1, buf) == -1){
    perror("send");
    exit(1);
  }

  struct Message msg;

  while(1){
    if(recv(serv_fd, &msg, sizeof msg, 0)){
      struct Message msgd;
      decode_msg(msg, &msgd);
      if(msgd.type == REGISTER_TAKEN){
        printf("This name is taken\n");
        exit(1);
      }else if(msgd.type == REGISTER_OK){
        printf("Got registered!\n");
        return;
      }
    }
  }
}
//******************

int calculate(char* expr, char* buf){
  if(strlen(expr) < 5)
  return -1;
  char* tmpexpr = malloc(strlen(expr) + 1);
  strcpy(tmpexpr, expr);
  char* arg1 = strtok(tmpexpr, " ");
  char* op = strtok(NULL, " ");
  char* arg2 = strtok(NULL, " ");

  if(!numbers_only(arg1) || !numbers_only(arg2)){
    return -1;
  }


  int arg1i = atoi(arg1);
  int arg2i = atoi(arg2);
  int resi;
  if(streq(op, "+"))
    resi = arg1i + arg2i;
  else if(streq(op, "-"))
    resi = arg1i - arg2i;
  else if(streq(op, "*"))
    resi = arg1i * arg2i;
  else if(streq(op, "/")){
    if(arg2i == 0){
      fprintf(stderr, "%s\n", "0 division!");
      exit(-1);
    }
    resi = arg1i / arg2i;
  }else{
    fprintf(stderr, "%s\n", "Unknown");
    return -1;
  }

  sprintf(buf, "%s = %d", expr, resi);
  return 0;

}


//main loop that waits for incoming messages
void run(){

  while(1){
    struct Message resp;
    if(recv(serv_fd, &resp, sizeof resp, 0)){
      struct Message respd;
      decode_msg(resp, &respd);
      if(respd.type == PING){ //ping
        printf("Pinged\n");
        send_msg(serv_fd, PING, -1, NULL);
        continue;
      }
      if(respd.type == CALC_EXPR){
        char* calcText = strtok(respd.expr, ":");
        char* orderNo = strtok(NULL, ":");
        char resBuf[MAX_MSG_LEN];
        if(calculate(calcText, resBuf) == -1){
          fprintf(stderr, "Unknown query\n");
          continue;
        }
        char toSend[MAX_MSG_LEN];
        sprintf(toSend, "o:[orderNo %s] %s\n", orderNo, resBuf);
        if(send_msg(serv_fd, OP, respd.orderNo, toSend) == -1){
          perror("send");
        }
      }
      else{
        printf("Got unknown type: %d\n", respd.type);
      }
    }
  }
}
//******************


void cleanUp(int signum){
  close(serv_fd);
  exit(0);
}





int main(int argc, char* argv[]){
  parse(argc, argv, name, &communicationWay, char_address, port);
  serv_fd = prepareSocket(communicationWay);
  if(serv_fd == -1)
    return -1;
  signal(SIGINT, cleanUp);

  sendRegisterMsg();
  run();

  while(1){}
}
