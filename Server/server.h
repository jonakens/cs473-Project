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
  ST_QUESTION,
  ST_ANSWER,
  ST_CHAT,
} status_st;

typedef enum {
  T_STOP,
  T_HELP,
  T_WHO,
  T_LOOK,
  T_LOGOUT,
  T_LOGIN,
  T_REGISTER,
  T_PASSWD,
  T_PRIVATE,
  T_GROUP,
  T_EXIT,
  T_QUIZ,
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
  char *answer;
  MESS *ibuf;
  MESS *obuf;
  time_t login_time;
  struct node *link;
} NODE;

typedef struct group {
  char *name;
  char *key;
  NODE *member;
  struct group *link;
} GROUP;

void set_priority ();
void init_database ();
int init_socket (int port);
int new_connection (int local_sock);

void open_stuff (int local_sock);
void write_stuff ();
void read_stuff ();
void process_stuff ();
void remove_stuff ();

NODE *make_node (int sock);
MESS *make_message ();
GROUP *make_group (char group_name[], char group_key[]);

void send_to_ibuf (NODE *target_user, char message[]);
void send_to_obuf (NODE *target_user, char message[]);
void remove_ibuf (NODE *user);
void remove_obuf (NODE *user);
void free_ibuf (NODE *user);
void free_obuf (NODE *user);

token_t lex_string (char line[], char args[]);
token_t special_check (char token[]);

void exit_nicely ();
int count_users ();
void print_prompt (GROUP *group, NODE *user);

void command_handler(GROUP *group, NODE *user, token_t tok, char *args);
void print_help(NODE *user);
void who_is_online (GROUP *current_group, NODE *target_user);
void private_message (NODE *source_user, char argument[]);
GROUP *manage_group (GROUP *current_group, NODE *user, char argument[]);
void get_question(GROUP *current_group, NODE *user);
void validate_answer (GROUP *current_group, NODE *user, char answer[]);

int check_length (NODE *user, char input[], char error[]);
int check_combination (NODE *user, char input[], char error[]);
void parse_args (status_st stat, char line[], char arg1[], char arg2[], char arg3[]);
void check_credentials (NODE *user, char credentials[]);
int find_user (char username[]);
void check_password (NODE *user, char password[]);
char *validate_password (char *username, char password[]);
char *make_password(char *username, char password[]);
void change_password (NODE *user, char credentials[]);
int update_password (char *username, char password[]);
