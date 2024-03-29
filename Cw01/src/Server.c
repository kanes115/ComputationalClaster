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
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);
    if ((unix_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Unix socket");
        return -1;
    }
    if ((bind(unix_socket, (const struct sockaddr *) &address, sizeof(address))) < 0) {
        close(unix_socket);
        perror("Unix socket bind");
        return -1;
    }
    if ((listen(unix_socket, 5)) < 0) {
        perror("Unix socket listen");
        return -1;
    }
    if ((make_socket_non_blocking(unix_socket)) < 0) {
        perror("Unix socket make non blocking");
        return -1;
    }
    return unix_socket;
}

int prepareWebSocket(char *port) {
    int web_socket;
    struct addrinfo name, *res;
    memset(&name, 0, sizeof(name));
    name.ai_family = AF_UNSPEC;
    name.ai_socktype = SOCK_STREAM;
    name.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, port, &name, &res);
    if ((web_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("Web socket");
        return -1;
    }
    if ((bind(web_socket, res->ai_addr, res->ai_addrlen)) < 0) {
        close(web_socket);
        perror("Web socket bind");
        return -1;
    }
    if ((listen(web_socket, 5)) < 0) {
        perror("Web socket listen");
        return -1;
    }
    if ((make_socket_non_blocking(web_socket)) < 0) {
        perror("Web socket make non blocking");
        return -1;
    }
    return web_socket;
}
//******************


int send_msg(int client, char *msg) {
    char message[MAX_MSG_LEN];
    sprintf(message, "%s", msg);
    if(send(client, message, MAX_MSG_LEN, 0) == -1){
      return -1;
    }
    return 0;
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
    if(send_msg(ptr->sock_fd, msg) == -1){
      printf("This socket is closed. Will be removed by piinger.");
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

  if(send_msg(ptr->sock_fd, msg) == -1){
    printf("This socket is closed. Will be removed by piinger.");
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
  char* buf = malloc(MAX_MSG_LEN);

  read(source_fd, buf, MAX_MSG_LEN);

  //printf("Got message: %s\n", buf);

  int type = buf[0];
  buf += 2;

  if(type == 'r'){    //type register
    if(existsClient(buf)){
      char* buff = malloc(MAX_MSG_LEN);
      sprintf(buff, "r:1");
      send_msg(source_fd, buff);
    }else{
      struct Client *cl = malloc(sizeof(struct Client));
      cl->sock_fd = source_fd;
      strcpy(cl->name, buf);
      cl->last_response = time(NULL);
      addClient(cl);
      char* buff = malloc(MAX_MSG_LEN);
      sprintf(buff, "r:0");
      if(send_msg(source_fd, buff) == -1){
        perror("send");
      }
    }
    return;
  }
  if(type == 'o'){
    printf("[clientId %d] %s\n", source_fd, buf);
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
                send_msg(clients[i]->sock_fd, buf);
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

    sprintf(buf, "o:%s:%d", str, ordersCounter++);
    sendToClient(buf);
  }

  return NULL;
}
//******************


//main net thread body
void* listenOnSockets(){

  int EFD;
  struct epoll_event event;
  struct epoll_event *events;

  listen(net_socket, backlog);
  listen(local_socket, backlog);

  EFD = epoll_create1(0);

  event.data.fd = net_socket;
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl (EFD, EPOLL_CTL_ADD, net_socket, &event);
  event.data.fd = local_socket;
  epoll_ctl (EFD, EPOLL_CTL_ADD, local_socket, &event); //my

  /* Buffer where events are returned */
  events = calloc (EVENTS_MAX_AMOUNT, sizeof event);

  while (1){
      int n;

      n = epoll_wait (EFD, events, EVENTS_MAX_AMOUNT, -1);
      for (int i = 0; i < n; i++){
	       if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))){
            //Simply error - the other end closed
	          fprintf (stderr, "this file descriptor is already closed on the other side\n");
	          close(events[i].data.fd);
            //deleteClient(events[i].data.fd);
	          continue;
          }
	        if(net_socket == events[i].data.fd || local_socket == events[i].data.fd){
            int curr_sock = events[i].data.fd;
              /* We have a notification on the listening socket, which
                 means one or more incoming connections. */
              while (1){
                  struct sockaddr in_addr;
                  socklen_t in_len;
                  int infd;

                  in_len = sizeof in_addr;
                  infd = accept (curr_sock, &in_addr, &in_len);

                  if (infd == -1){
                      if ((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                          /* We have processed all incoming
                             connections. */
                          break;
                      }
                      else{
                          perror ("accept");
                          break;
                        }
                  }

                  /* Make the incoming socket non-blocking and add it to the
                     list of fds to monitor. */
                  if(make_socket_non_blocking(infd) == -1){
                    abort ();
                  }

                  event.data.fd = infd;
                  event.events = EPOLLIN | EPOLLET;
                  if(epoll_ctl (EFD, EPOLL_CTL_ADD, infd, &event) == -1){
                      perror ("epoll_ctl");
                      abort ();
                  }
              }
              continue;
          }
          else{
              /* We have data on the fd waiting to be read. Read and
                 display it. We must read whatever data is available
                 completely, as we are running in edge-triggered mode
                 and won't get a notification again for the same
                 data. */
                 serveDataMsg(events[i].data.fd);

        }
    }

    //free (events);
    //close (net_socket);
    //close (local_socket);
  }
}
//******************

int main(int argc, char* argv[]) {
  srand(time(NULL));
  parse(argc, argv, &port, path);

  char portbuf[7];
  sprintf(portbuf, "%d", port);
  net_socket = prepareWebSocket(portbuf);
  local_socket = prepareUnixSocket(path);
  if(net_socket == -1 || local_socket == -1)
    exit(-1);
  make_socket_non_blocking(net_socket);
  make_socket_non_blocking(local_socket);

  signal(SIGINT, cleanUp);


  pthread_create(&p1, NULL, (void*) listenOnSockets, NULL);
  pthread_create(&p2, NULL, (void*) opsIn, NULL);
  pthread_create(&p3, NULL, (void*) sendPingMsg, NULL);

  while(1){}

}
