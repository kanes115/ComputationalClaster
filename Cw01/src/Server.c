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





//main thread body
void* listenOnSockets(){

  int s;
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
            curr_sock = events[i].data.fd;
              /* We have a notification on the listening socket, which
                 means one or more incoming connections. */
              while (1){
                  struct sockaddr in_addr;
                  socklen_t in_len;
                  int infd;
                  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

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

                  if(getnameinfo (&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV) == 0){
                                printf("Accepted connection on descriptor %d "
                             "(host=%s, port=%s)\n", infd, hbuf, sbuf);
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
              int done = 0;

              while (1){
                  ssize_t count;
                  char buf[512];

                  count = read (events[i].data.fd, buf, sizeof buf);
                  if (count == -1)
                    {
                      /* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
                      if (errno != EAGAIN)
                        {
                          perror ("read");
                          done = 1;
                        }
                      break;
                    }
                  else if (count == 0)
                    {
                      /* End of file. The remote has closed the
                         connection. */
                      done = 1;
                      break;
                    }

                  /* Write the buffer to standard output */
                  s = write (1, buf, count);
                  if (s == -1)
                    {
                      perror ("write");
                      abort ();
                    }
                }

              if (done)
                {
                  printf ("Closed connection on descriptor %d\n",
                          events[i].data.fd);

                  /* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
                  close (events[i].data.fd);
                }
            }
        }
    }

  free (events);
  close (sfd);



  // int common_fd = epoll_create1(0);
  // struct epoll_event evt_hint;
  // evt_hint.events = EPOLLIN | EPOLLOUT;
  // evt_hint.data.fd = local_socket;
  // epoll_ctl(common_fd, EPOLL_CTL_ADD, local_socket, &evt_hint);
  // evt_hint.data.fd = net_socket;
  // epoll_ctl(common_fd, EPOLL_CTL_ADD, net_socket, &evt_hint);
  //
  // struct epoll_event evts[EVENTS_MAX_AMOUNT];
  //
  // epoll_wait(common_fd, evts, EVENTS_MAX_AMOUNT, -1);
  //
  // return NULL;
}


int main(int argc, char* argv[]) {
  parse(argc, argv, port, path);

  net_socket = prepareSocket(AF_INET);
  local_socket = prepareSocket(AF_LOCAL);
  make_socket_non_blocking(net_socket);
  make_socket_non_blocking(local_socket);

  signal(SIGINT, cleanUp);

  pthread_t p_id;
  pthread_create(&p_id, NULL, (void*) listenOnSockets, NULL);

  getchar();

}
