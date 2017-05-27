#define _XOPEN_SOURCE
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>



#include "Definitions.h"

//Globals
int orderCounter = 0;
int port;
char path[MAX_PATH_LEN];

int local_socket, net_socket;
int backlog = 5;
//***

//Parse
void showUsageClose(){
  fprintf(stderr, "%s\n", "Usage:\n./Server <port> <path>");
  exit(-1);
}

void parse(int argc, char* argv[], int* port, char* path){
  if(argc != 3)
    showUsageClose();

  if(strlen(argv[1]) > 5)
    showUsageClose();
  if(strlen(argv[2]) > MAX_PATH_LEN - 1)
    showUsageClose();

  *port = atoi(argv[1]);
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

int prepareSockets(int sockType){
  struct sockaddr_in server;

  //Create socket
  net_socket = socket(sockType , SOCK_STREAM , 0);
  if (net_socket == -1){
      printf("Could not create socket");
      return -1;
  }

  //Prepare the sockaddr_in structure
  server.sin_family = sockType;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( port );

  //Bind
  if( bind(net_socket, (struct sockaddr *)&server , sizeof(server)) < 0){
      perror("bind failed. Error");
      return 1;
  }

  //Listen
  listen(net_socket , backlog);

  return 0;

}

void* listenOnSockets(){
  int common_fd = epoll_create1(0);
  struct epoll_event evt_hint;
  evt_hint.events = EPOLLIN | EPOLLOUT;
  evt_hint.data.fd = local_socket;
  epoll_ctl(common_fd, EPOLL_CTL_ADD, local_socket, &evt_hint);
  evt_hint.data.fd = net_socket;
  epoll_ctl(common_fd, EPOLL_CTL_ADD, net_socket, &evt_hint);

  struct epoll_event evts[EVENTS_MAX_AMOUNT];

  epoll_wait(common_fd, evts, EVENTS_MAX_AMOUNT, -1);

  return NULL;
}




int main(int argc, char* argv[]) {
  parse(argc, argv, &port, path);
  prepareSockets(AF_INET);
  prepareSockets(AF_LOCAL);
  signal(SIGINT, cleanUp);

  pthread_t p_id;
  pthread_create(&p_id, NULL, (void*) listenOnSockets, NULL);

  getchar();

}
