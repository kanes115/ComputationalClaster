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
#define PING 4
#define REGISTER 1
#define REGISTER_OK 1
#define REGISTER_TAKEN 2
#define CALC_EXPR 5


#define CONNECTION_TIMEOUT 15
#define PING_INTERVAL 5


struct Client{
  int sock_fd;
  char name[CLIENTS_MAX_NAMELEN];
  time_t last_response;

  struct sockaddr addr;
  socklen_t len;
};

struct Message{
  int type;
  int orderNo;
  char expr[MAX_MSG_LEN];
};
