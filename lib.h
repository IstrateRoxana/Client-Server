#ifndef LIB
#define LIB

#define HOST "127.0.0.1"
#define PORT1 10000
#define PORT2 10001

#define MSGSIZE 1400

typedef struct {
  int len;
  char payload[MSGSIZE];
} msg;

void init(char* remote,int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
int recv_message_timeout(msg *m, int timeout);

#endif

