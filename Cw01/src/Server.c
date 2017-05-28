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
#include <errno.h>
#include <pthread.h>
#include <string.h>



#include "Definitions.h"

//Globals
int orderCounter = 0;
int port;
char path[MAX_PATH_LEN];

int local_socket, net_socket;
int backlog = 5;

int ordersCounter = 0;
//klienci
struct Client* clients[CLIENTS_MAX_NO];
int clientsCounter = 0;
pthread_mutex_t clients_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
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


int prepareSocket(int sockType){
  struct sockaddr_in serv_addr;
  int listenfd;

  listenfd = socket(sockType, SOCK_STREAM, 0);
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = sockType;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

  return listenfd;
}
//***




//Client menaging
void addClient(struct Client* cl){
  clients[clientsCounter++] = cl;
}

void sendToClient(char* msg){
  if(clientsCounter == 0)
    return;

  int cl_no = rand()%clientsCounter;

  if(write(clients[cl_no]->sock_fd, msg, strlen(msg) + 1) = -1){
    fprintf(stderr, "%s\n", "write...");
  }
}

int existsClient(char* name){
  for(int i = 0; i < clientsCounter; i++){
    if(strcmp(clients[i]->name, name) == 0)
      return 1;
  }
  return 0;
}
//***




void serveDataMsg(int source_fd){
  char* buf = malloc(MAX_MSG_LEN);

  read(source_fd, buf, MAX_MSG_LEN);

  printf("Got message: %s\n", buf);

  int type = buf[0];
  buf += 2;

  if(type == 'r'){    //type register
    printf("Was r!\n");
    if(existsClient(buf)){
      char* buff = malloc(MAX_MSG_LEN);
      sprintf(buff, "r:1");
      send(source_fd, buff, MAX_MSG_LEN, 0);
    }else{
      printf("And client does not exist!\n");
      struct Client *cl = malloc(sizeof(struct Client));
      cl->sock_fd = source_fd;
      strcpy(cl->name, buf);
      addClient(cl);
      char* buff = malloc(MAX_MSG_LEN);
      sprintf(buff, "r:0");
      if(send(source_fd, buff, MAX_MSG_LEN, 0) == -1){
        perror("send");
      }
      printf("sent\n");
    }
    return;
  }

  if(type == 'o'){
    printf("[clientId %d] %s\n", source_fd, buf);
    return;
  }
}





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
    printf("sending: %s\n", buf);
    sendToClient(buf);
  }

  return NULL;
}




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
            //Simply error
	          fprintf (stderr, "epoll error\n");
	          close (events[i].data.fd);
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


int main(int argc, char* argv[]) {
  srand(time(NULL));
  parse(argc, argv, &port, path);

  net_socket = prepareSocket(AF_INET);
  local_socket = prepareSocket(AF_LOCAL);
  make_socket_non_blocking(net_socket);
  make_socket_non_blocking(local_socket);

  signal(SIGINT, cleanUp);

  pthread_t p_id;
  pthread_create(&p_id, NULL, (void*) listenOnSockets, NULL);
  pthread_create(&p_id, NULL, (void*) opsIn, NULL);

  while(1){}

}
