//Operators
#define OP_ADD 0
#define OP_SUB 1
#define OP_MUL 2
#define OP_DIV 3

//limits
#define CLIENTS_MAX_NO 20
#define CLIENTS_MAX_NAMELEN 32
#define EVENTS_MAX_AMOUNT 10
#define MAX_MSG_LEN 64

//communication ways
#define MAX_PATH_LEN 104  //for unix 108 in fact, working on macos
#define OP_MSG_LEN 10

//types of messages
#define PING 0
#define REGISTER 1
#define REGISTER_OK 1
#define REGISTER_TAKEN 2
#define OP 3

struct Client{
  int sock_fd;
  char name[CLIENTS_MAX_NAMELEN];
};

struct Operation{
  int arg1;
  int arg2;
  int op;
};
