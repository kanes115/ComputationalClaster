#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "Definitions.h"

//Globals
int orderCounter = 0;
char port[6];
char path[MAX_PATH_LEN];

int local_socket, net_socket;
//***

//Parse
void showUsageClose(){
  fprintf(stderr, "%s\n", "Usage:\n./Server <port> <path>");
  exit(-1);
}

void parse(int argc, char* argv[], char* port, char* path){
  if(argc != 3)
    showUsageClose();

  if(strlen(argv[1]) > 5)
    showUsageClose();
  if(strlen(argv[2]) > MAX_PATH_LEN - 1)
    showUsageClose();

  strcpy(port, argv[1]);
  strcpy(path, argv[2]);
}
//***

//cleanUp
void cleanUp(int signum){
  shutdown(local_socket, SHUT_RDWR);
  shutdown(net_socket, SHUT_RDWR);
  close(local_socket);
  close(net_socket);
}
//***

int prepareSockets(){
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;  // use IPv4
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


  if(getaddrinfo(NULL, port, &hints, &res) != 0){
      perror("getaddrinfo");
      return -1;
  }
  if((net_scoket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
      perror("socket");
      return -2;
  }
  if(bind(net_scoket, res->ai_addr, res->ai_addrlen) != 0){
        perror("bind");
        return -3;
    }
  if(listen(net_scoket, backlog) != 0){
      perror("listen");
      return -4;
  }


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_LOCAL;  // use local
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  if(getaddrinfo(NULL, port, &hints, &res) != 0){
      perror("getaddrinfo");
      return -1;
  }
  if((local_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
      perror("socket");
      return -2;
  }
  if(bind(local_socket, res->ai_addr, res->ai_addrlen) != 0){
        perror("bind");
        return -3;
    }
  if(listen(local_socket, backlog) != 0){
      perror("listen");
      return -4;
  }
  return 0;
}

void* listenOnSockets(){
  common_fd = epoll_create1(0);
  struct epoll_event evt_hint;
  evt_hint.events = EPOLLIN | EPOLLOUT;
  evt_hint.data.fd = local_socket;
  epoll_ctl(common_fd, EPOLL_CTL_ADD, local_socket, &evt_hint);
  evt_hint.data.fd = net_socket;
  epoll_ctl(common_fd, EPOLL_CTL_ADD, net_socket, &evt_hint);

  struct epoll_event evts[EVENTS_MAX_AMOUNT];

  epoll_wait(common_fd, &evts, EVENTS_MAX_AMOUNT, -1);
}


int main(int argc, char* argv[]) {
  parse(argc, argv, port, path);
  prepareSockets();
  signal(SIGINT, cleanUp);

  pthread_create(NULL, NULL, (void*) listenOnSockets, NULL);

  getchar();

}
