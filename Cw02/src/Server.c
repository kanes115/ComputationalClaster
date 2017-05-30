#define _BSD_SOURCE
#define  _XOPEN_SOURCE 600
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
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <poll.h>
#include <sys/socket.h>
       #include <netinet/in.h>
       #include <arpa/inet.h>




#include "Definitions.h"

//Globals
int orderCounter = 0;
int port;
char path[MAX_PATH_LEN];

int local_socket, net_socket;
int backlog = 5;

int ordersCounter = 0;

pthread_t p1, p2, p3;
//klienci
struct Client* clients[CLIENTS_MAX_NO];
pthread_mutex_t clients_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
//******************

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

int streq(char *string1, char *string2) {
    return strcmp(string1, string2) == 0;
}
//******************

//cleanUp
void cleanUp(int signum){
  pthread_cancel(p1);
  pthread_cancel(p2);
  pthread_cancel(p3);

  if(shutdown(local_socket, SHUT_RDWR) == -1){
    printf("Couldn't close\n");
  }
  if(shutdown(net_socket, SHUT_RDWR) == -1){
    printf("Couldn't close\n");
  }
  for(int i = 0; i < CLIENTS_MAX_NO; i++){
    if(clients[i] != NULL){
      if(shutdown(clients[i]->sock_fd, SHUT_RDWR) == -1){
        printf("Couldn't close\n");
      }
      if(close(clients[i]->sock_fd) == -1){
        printf("Couldn't close\n");
      }
    }
  }
  if(close(local_socket) == -1){
    printf("Couldn't close\n");
  }
  unlink(path);
  if(close(net_socket) == -1){
    printf("Couldn't close\n");
  }
  exit(0);
}
//******************

//preparation

static int make_socket_non_blocking(int sfd){
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1){
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1){
      perror ("fcntl");
      return -1;
    }

  return 0;
}




int prepareUnixSocket(char *path) {
    int unix_socket;
    unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

    struct sockaddr_un UNIX_addr;
    UNIX_addr.sun_family = AF_UNIX;
    strcpy(UNIX_addr.sun_path, path);


    if(bind(unix_socket, (struct sockaddr *)&UNIX_addr, sizeof(UNIX_addr))){
      perror("Error binding");
      exit(-1);
    }
    if ((make_socket_non_blocking(unix_socket)) < 0) {
        perror("Unix socket make non blocking");
        return -1;
    }
    return unix_socket;
}

int prepareWebSocket(int port) {
    int web_socket;
    web_socket = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in IP_addr;
    IP_addr.sin_family = AF_INET;
    IP_addr.sin_port = htons(port);
    IP_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(web_socket, (struct sockaddr *)&IP_addr, sizeof(IP_addr))){
      perror("Error binding");
      exit(-1);
    }
    if ((make_socket_non_blocking(web_socket)) < 0) {
        perror("Unix socket make non blocking");
        return -1;
    }
    return web_socket;
}
//******************




int send_msg(struct Client* cl, int type, int orderNo, char* expr) {
    // printf("sending: cl->sock_fd = %d, type = %d, orderNo = %d, expr = %s\n", cl->sock_fd, type, orderNo, expr);
    struct Message msg;
    msg.type = htonl(type);
    msg.orderNo = htonl(orderNo);
    if(!(expr == NULL))
      strcpy(msg.expr, expr);
    if(sendto(cl->sock_fd, &msg, sizeof(msg), MSG_NOSIGNAL, &(cl->addr), cl->len) == -1){
      perror("Send");
      return -1;
    }
    return 0;
}

void decode_msg(struct Message orig, struct Message* out){
  out->type = ntohl(orig.type);
  out->orderNo = ntohl(orig.orderNo);
  strcpy(out->expr, orig.expr);
}


//Client menaging
void addClient(struct Client* cl){
  struct Client* ptr = clients[0];
  int i = 0;
  while(ptr != NULL){
    i++;
    ptr = clients[i];
  }
  clients[i] = cl;
}

void sendToClient(char* msg){
  int cl_no = rand()%CLIENTS_MAX_NO;

  struct Client* ptr = clients[cl_no];
  if(ptr != NULL){
    if(send_msg(ptr, CALC_EXPR, orderCounter, msg) == -1){
      printf("This socket is closed. Will be removed by piinger.");
      return;
    }
    return;
  }
  int i = (cl_no + 1) % CLIENTS_MAX_NO;
  ptr = clients[i];
  while(ptr == NULL){
    i = (i + 1) % CLIENTS_MAX_NO;
    if(i == cl_no){
      fprintf(stderr, "%s\n", "No clients!");
      return;
    }
    ptr = clients[i];
  }

  if(send_msg(ptr, CALC_EXPR, orderCounter, msg) == -1){
    printf("This socket is closed. Will be removed by piinger.");
    return;
  }
}

int existsClient(char* name){
  for(int i = 0; i < CLIENTS_MAX_NO; i++){
    if(clients[i] != NULL && streq(clients[i]->name, name))
      return 1;
  }
  return 0;
}

void deleteClientAtInd(int ind){
  shutdown(clients[ind]->sock_fd, SHUT_RDWR);
  close(clients[ind]->sock_fd);
  free(clients[ind]);
  clients[ind] = NULL;
  printf("Removing client %d\n", ind);
}

struct Client* findClient(int socketid){
  for(int i = 0; i < CLIENTS_MAX_NO; i++){
    if(clients[i] != NULL && clients[i]->sock_fd == socketid)
      return clients[i];
  }
  return NULL;
}
//******************


//Functions used by net thread body
int update_client_timeout(int client_socket) {
    //TODO Add mutex
    for (int client = 0; client < CLIENTS_MAX_NO; ++client) {
        if (clients[client] != NULL) {
            if (clients[client]->sock_fd == client_socket) {
                clients[client]->last_response = time(NULL);
                return 0;
            }
        }
    }
    return -1;
}

void serveDataMsg(int source_fd){
  struct sockaddr addr;
  struct Message msg_in, msgin_d;
  struct Client sender;
  socklen_t len = sizeof(addr);

  if(recvfrom(source_fd, &msg_in, sizeof msg_in, MSG_WAITALL, &addr, &len) == -1){
    perror("recvfrom");
  }

  decode_msg(msg_in, &msgin_d);

  int type = msgin_d.type;

  sender.sock_fd = source_fd;
  sender.addr = addr;
  sender.len = len;

  if(type == REGISTER){    //type register
    if(existsClient(msgin_d.expr)){
      send_msg(&sender, REGISTER_TAKEN, -1, NULL);
    }else{
      struct Client *cl = malloc(sizeof(struct Client));
      cl->sock_fd = source_fd;
      strcpy(cl->name, msgin_d.expr);
      cl->last_response = time(NULL);
      cl->addr = addr;
      cl->len = len;
      addClient(cl);
      send_msg(cl, REGISTER_OK, -1, NULL);
    }
    return;
  }
  if(type == CALC_EXPR){
    printf("[clientId %d] %s\n", source_fd, msgin_d.expr);
    orderCounter++;
    return;
  }
  if(type == PING){
    update_client_timeout(source_fd);
  }
}
//******************



//main ping body - it pings clients but the net thread waits for respond
void* sendPingMsg(){
    while (1){
        for(int i = 0; i < CLIENTS_MAX_NO; ++i) {
            if (clients[i] != NULL) {
                char buf[2];
                sprintf(buf, "%c", PING);
                send_msg(clients[i], PING, -1, NULL);
                if (time(NULL) - clients[i]->last_response >= CONNECTION_TIMEOUT) {
                    deleteClientAtInd(i);
                }
            }
        }
        sleep(PING_INTERVAL);
    }
    return NULL;
}
//******************

//main stdin thread body
void* opsIn(){

  while (1) {
    char buf[MAX_MSG_LEN];
    char str[MAX_MSG_LEN - 10];
    printf(">> ");
    fgets(str, MAX_MSG_LEN - 10, stdin);

    int i = strlen(str)-1;
    if( str[ i ] == '\n')
      str[i] = '\0';

    sprintf(buf, "%s", str);
    sendToClient(buf);
  }

  return NULL;
}
//******************


//main net thread body
void* listenOnSockets(){

  int EFD;

  epoll_data_t fd_new;
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  EFD = epoll_create1(0);
  fd_new.fd = net_socket;
  event.data = fd_new;
  epoll_ctl(EFD, EPOLL_CTL_ADD, net_socket, &event);
  fd_new.fd = local_socket;
  event.data = fd_new;
  epoll_ctl(EFD, EPOLL_CTL_ADD, local_socket, &event);

  while (1){
      epoll_wait(EFD, &event, 1, -1);
       if ((event.events & EPOLLERR) || (event.events & EPOLLHUP) || (!(event.events & EPOLLIN))){
          fprintf (stderr, "this file descriptor is already closed on the other side\n");
          close(event.data.fd);
          continue;
        }
        else{
          serveDataMsg(event.data.fd);
      }
  }
}
//******************

int main(int argc, char* argv[]) {
  srand(time(NULL));
  parse(argc, argv, &port, path);

  net_socket = prepareWebSocket(port);
  local_socket = prepareUnixSocket(path);
  if(net_socket == -1 || local_socket == -1)
    exit(-1);

  signal(SIGINT, cleanUp);

  char hostname[250];
  gethostname(hostname, 250);
  struct hostent*  info= gethostbyname(hostname);
  printf("Server started:\n");
  printf("IP: %s, PORT: %d\n", inet_ntoa(*(struct in_addr*)(info->h_addr_list)), port);


  pthread_create(&p1, NULL, (void*) listenOnSockets, NULL);
  pthread_create(&p2, NULL, (void*) opsIn, NULL);
  pthread_create(&p3, NULL, (void*) sendPingMsg, NULL);

  while(1){}

}
