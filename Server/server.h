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

#define _XOPEN_SOURCE_EXTENDED 1
#include <sys/resource.h>

#define _XOPEN_SOURCE
#include <unistd.h>
#include <crypt.h> 

#define SHORTSTR    32
#define LONGSTR   4096
#define K         1024
#define PORT      3000
#define SLEN         8



#define MINNAME     2
#define MAXNAME    12
#define MINPASS     4
#define MAXPASS    20
#define CHARLIMIT  64

//user states
#define ST_BAD     0
#define ST_LOGIN   1
#define ST_PASSWD  2
#define ST_CHAT    3

#define PASSWD "passwd"

#define MESG_ON  1
#define MESG_OFF 0

#define COMMAND '/'

#define MAXCMDOBJS 5

char cmdline[MAXCMDOBJS][K];

typedef struct messagenode { 

  char *message;
  struct messagenode *messlink;

} MESS;

typedef struct node {
  
  char *uname;
  char *pass;
  char *epass;
  int desc;            // the read/write descriptor
  char ibuf[LONGSTR];  // i for Input
  char obuf[LONGSTR];  // o for Output
  MESS *head, *tail;
  int status;          // 1 is connected, 0 means remove this one
  int uflag; //for the username prompt
  int cflag;
  int mesg;
  char *addr;
  time_t loginTime;
  struct node *link;
  
} NODE;

int init_socket(int port);
int new_connection(int s);
void open_stuff(int s);
void read_stuff(int sock);
void process_stuff(int sock);
void write_stuff(int sock);
void remove_stuff(int sock);
NODE *makenode(int desc, char addr[]);
int countusers();
void printoutput(MESS *p);
MESS *removehead(MESS *h);
void exit_nicely();
void sendToChat(NODE *p, char *s);
void sendToUser(NODE *p, char *s);
MESS *createMessage(char mess[]);
void sendMessage(NODE *user);
int checkname(char s[]);
int checkpasswd(char *user, char *plaintext);
int checkpass(char *key, char *plaintext, char *ciphertext);
char *finduser(char *key, char epass[]);
char *makepass(char *key, char *plaintext);
int isUserRegistered(char *key);
void commandHandler(char *cmd, NODE *user);
