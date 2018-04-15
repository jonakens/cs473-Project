/* run on cs server */
/* nice 15 server >& log & */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <sqlite3.h>

#define _XOPEN_SOURCE_EXTENDED 1
#include <sys/resource.h>

#define _XOPEN_SOURCE
#include <unistd.h>
#include <crypt.h>

#define SHORTSTR    32
#define LONGSTR   4096
#define K         1024
#define PORT      3000

typedef enum {
  ST_BAD,
  ST_LOGIN,
  ST_PASSWD,
  ST_CHAT,
  ST_NEWUSR,
  ST_NEWPWD,
  ST_MENU
} status_st;

typedef struct messagenode {
  char *message;
  struct messagenode *messlink;
} MESS;

typedef struct node {
  int desc;
  status_st status;
  char *name;
  char *hash;
  char *addr;
  MESS *ibuf;
  MESS *obuf;
  time_t login_time;
  struct node *link;
} NODE;

typedef enum {
  T_QUIT,
  T_HELP,
  T_LOGIN,
  T_LOGOUT,
  T_NEW,
  T_WHO,
  T_OTHER
} token_t;

void set_priority ();
void init_database ();
int init_socket (int port);
void open_stuff (int s);
int new_connection (int s);
NODE *make_node (int desc);
MESS *make_message ();
void send_to_obuf (NODE *target, char message[]);
void send_to_ibuf (NODE *target, char message[]);
void write_stuff (int sock);
void remove_ibuf (NODE *nd);
void remove_obuf (NODE *nd);
void free_ibuf (NODE *nd);
void free_obuf (NODE *nd);
void read_stuff (int sock);
void process_stuff (int sock);
void remove_stuff (int sock);
token_t lex_string (char line[]);
token_t special_check (char token[]);
void exit_nicely ();
int count_users ();
void command_handler(NODE *user, token_t cmd);
void print_help(NODE *user);
void check_username (status_st status, NODE *p, char username[]);
void check_password (status_st status, NODE *user, char password[]);
