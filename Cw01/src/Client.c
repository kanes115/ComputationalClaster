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
#include <signal.h>
#include <ctype.h>



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


void send_msg(int client, char *msg) {
    char message[MAX_MSG_LEN];
    sprintf(message, "%s", msg);
    send(client, message, MAX_MSG_LEN, 0);
}



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
            perror("net socket");
            return -1;
        }
        if (connect(server_socket, res->ai_addr, res->ai_addrlen) == -1) {
            close(server_socket);
            perror("net connection");
            return -1;
        }
    } else if (communicationWay == AF_LOCAL) {
        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, char_address);
        if ((server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("local socket");
            return -1;
        }
        if (connect(server_socket, (const struct sockaddr *) &address, sizeof(address)) == -1) {
            close(server_socket);
            perror("local connection");
            return -1;
        }
    } else {
        server_socket = -1;
    }
    return server_socket;
}
//***

//messages

void sendRegisterMsg(){
  char* buf = malloc(MAX_MSG_LEN);
  sprintf(buf, "r:%s", name);
  printf("Sending my name in msg: %s (length = %ld)\n", buf, strlen(buf) + 1);
  if(send(serv_fd, buf, strlen(buf) + 1, 0) == -1){
    perror("send");
    exit(1);
  }

  char* resp = malloc(MAX_MSG_LEN);

  while(1){
    if(recv(serv_fd, resp, MAX_MSG_LEN, 0)){
      if(streq(resp, "r:1")){
        printf("This name is taken\n");
        exit(1);
      }else if(streq(resp, "r:0")){
        printf("Got registered!\n");
        return;
      }
    }
  }
}

//***

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





void run(){

  while(1){
    char* resp = malloc(MAX_MSG_LEN);
    if(recv(serv_fd, resp, MAX_MSG_LEN, 0)){
      printf("%s\n", resp);
      if(resp[0] == PING){ //ping
        printf("Pinged\n");
        char buftmp[2];
        sprintf(buftmp, "%c", PING);
        send_msg(serv_fd, buftmp);
        continue;
      }
      if(resp[0] == 'o'){
        resp += 2;
        char* calcText = strtok(resp, ":");
        char* orderNo = strtok(NULL, ":");
        char resBuf[MAX_MSG_LEN];
        if(calculate(calcText, resBuf) == -1){
          fprintf(stderr, "Unknown query\n");
          continue;
        }
        char toSend[MAX_MSG_LEN];
        sprintf(toSend, "o:[orderNo %s] %s\n", orderNo, resBuf);
        if(send(serv_fd, toSend, MAX_MSG_LEN, 0) == -1){
          perror("send");
        }
      }
      else{
        printf("Got unknown: %s\n", resp);
      }
    }
  }
}




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
