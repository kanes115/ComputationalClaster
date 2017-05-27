#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>

#include "Definitions.h"


//Parse
void showUsageClose(){
  fprintf(stderr, "%s\n", "Usage:\n./Client <name> <local|net> <ipv4|path>");
  exit(-1);
}

void parse(int argc, char* argv[], char* name, int* communicationWay, char* address){
  if(argc != 4)
    showUsageClose();

  strcpy(name, argv[1]);

  if(strcmp(argv[2], "local") == 0)
    *communicationWay = AF_LOCAL;
  else if(strcmp(argv[2], "ipv4") == 0)
    *communicationWay = AF_LOCAL;
  else
    showUsageClose();

  strcpy(address, argv[3]);
}
//***




int main(int argc, char* argv[]){
  char name[CLIENTS_MAX_NAMELEN];
  int communicationWay;
  char address[MAX_PATH_LEN];
  parse(argc, argv, name, &communicationWay, address);


}
