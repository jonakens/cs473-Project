/* run on cs server */
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
#define MAX         20
#define MIN          2

typedef enum {
  ST_BAD,
  ST_MENU,
  ST_LOGIN,
  ST_REGISTER,
  ST_PASSWD,
  ST_PRIVATE,
  ST_GROUP,
  ST_EXIT,
  ST_CHAT
} status_st;

typedef enum {
  T_STOP,
  T_HELP,
  T_WHO,
  T_LOGOUT,
  T_LOGIN,
  T_REGISTER,
  T_PASSWD,
  T_PRIVATE,
  T_GROUP,
  T_EXIT,
  T_OTHER
} token_t;

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

typedef struct group {
  char *name;
  NODE *memlist;
  struct group *link;
} GROUP;

void set_priority ();
void init_database ();
int init_socket (int port);
int new_connection (int s);

void open_stuff (int s);
void write_stuff (int sock);
void read_stuff (int sock);
void process_stuff (int sock);
void remove_stuff (int sock);

NODE *make_node (int desc);
MESS *make_message ();
GROUP *make_group (char groupname[]);

void send_to_ibuf (NODE *target, char message[]);
void send_to_obuf (NODE *target, char message[]);
void remove_ibuf (NODE *nd);
void remove_obuf (NODE *nd);
void free_ibuf (NODE *nd);
void free_obuf (NODE *nd);

token_t lex_string (char line[], char args[]);
token_t special_check (char token[]);

void exit_nicely ();
int count_users ();
void print_prompt (GROUP *grp, NODE *usr);

void command_handler(NODE *user, token_t cmd, char *args);
void print_help(NODE *user);
void who_is_online (NODE *user);
void private_message (NODE *user, char argument[]);
GROUP *manage_group (GROUP *gnode, NODE *user, char argument[]);

int check_length (NODE *user, char input[], char error[]);
int check_combination (NODE *user, char input[], char error[]);
void parse_args (status_st stat, char line[], char arg1[], char arg2[], char arg3[]);
void check_credentials (status_st status, NODE *p, char credentials[]);
int find_user (char username[]);
void check_password (status_st status, NODE *user, char password[]);
char *validate_password (char *username, char key[]);
char *make_password(char *username, char key[]);
void change_password (NODE *user, char credentials[]);
int update_password (char *username, char key[]);
