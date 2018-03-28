#include "server.h"

NODE *head = 0;

int maxdesc;
struct timeval delay,zero;

int main(int argc, char *argv[])
{ 
  int port;

  int priority, which;
  which = PRIO_PROCESS;
  priority = 19;
  
  pid_t who;
  who = getpid();

  int ret = setpriority(which, who, priority);
  if(ret < 0){
    fprintf(stderr, "[Priority] Priority not set successfuly");
    exit(1);
  }
  fprintf(stderr, "[Priority] Priority was set to %d sucessfully.\n",getpriority(which, who));
  if(argc == 2){
    port = atoi(argv[1]);
  } else {
    port = PORT;
  }
  int s;
  time_t t;
  srandom(getpid());
  signal(SIGINT, exit_nicely);
  s = init_socket(port);

  fprintf(stderr,"socket = %d, port: %d\n", s, port);
  
  t = time(0);

  for(;;){
    open_stuff(s);                 // check for new connections

    read_stuff(s);                 // any input from connected users?
    process_stuff(s);              // do whatever this server does
    write_stuff(s);

    remove_stuff(s);
    usleep(100);
    if(time(0) - t >= 5){
      t = time(0);
      fprintf(stderr,"%d users connected.\n", countusers());
    }
  }
}
int init_socket(int port)
{
  int s;
  char *opt, hostname[LONGSTR];
  struct sockaddr_in sa;

/*
 * fill out the address structure, as in the client
 */
  memset(&sa,0,sizeof(sa));   // zero means local host in some field
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(port);
/*
 * create the socket
 */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("Init-socket");
    exit(1);
  }

/*
 * ask OS for all packets sent to the port
 */

  if(bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    perror("bind");
    close(s);
    exit(1);
  }
/*
 * listen for new connections -- queue at most 4 connections (?) */
  listen(s, 256);
  return(s);
}
/*
 * this functions gets the descriptor we use for each new
 * connection to our server
 */
int new_connection(int s)
{
  struct sockaddr isa;
  int size,t;

  size = sizeof(isa);
  getsockname(s,&isa,&size);
  if((t = accept(s,&isa,&size)) < 0){
    perror("Accept");
    return -1;
  }
  return t;
}
void open_stuff(int s)
{
  int n,desc,size;
  struct sockaddr_in sock;
  fd_set iset;
  unsigned int t,x;
  char buf[LONGSTR], *q;
  char addr[SHORTSTR];
  NODE *p;

/*
 * ask if there is data to read from the socket.  we don't actually read anything
 * from the socket.  that's just a signal that someone wants to connect to our
 * server
 */
  FD_ZERO(&iset);
  FD_SET(s,&iset);
  select(s+1,&iset,0,0,&zero);
  if(!FD_ISSET(s,&iset))
    return;
/*
 * new_connection returns the descriptor we use for reading/writing
 */
  desc = new_connection(s);
  if(desc < 0)
    return;    
  size = sizeof(sock);
  if(getpeername(desc,(struct sockaddr *)&sock,&size) < 0){
    perror("getpeername");
    close(desc);
    return;
  }
  strncpy(addr,(char *)inet_ntoa(sock.sin_addr),SHORTSTR);
  if(head == NULL){
    head = makenode(desc,addr);
  } else {
    p = makenode(desc,addr);
    p->link = head;
    head = p;
  }
}
NODE *makenode(int desc, char addr[])
{
   NODE *p;  // pointer to new node

   p = malloc(sizeof(NODE));
   p->desc = desc;
   p->status = ST_LOGIN;
   *p->ibuf  = 0;
   *p->obuf = 0;
   p->addr = strdup(addr);
   p->link = NULL;
   p->mesg = MESG_ON;
   return p;
}
void read_stuff(int sock)
{
   NODE *p;
   fd_set iset;
   int dmax = 0;
   int nbytes;             // number of bytes read
   struct timeval zero;
   char buffer[LONGSTR];

   zero.tv_sec = 0;
   zero.tv_usec = 0;
   FD_ZERO(&iset);
   for(p = head ; p != NULL ; p = p->link){
     FD_SET(p->desc, &iset);
     if(p->desc > dmax)
       dmax = p->desc;
   }
   select(dmax+1, &iset, 0, 0, &zero);
   for(p = head ; p != NULL ; p = p->link){
     if(FD_ISSET(p->desc, &iset)){
       nbytes = read(p->desc, buffer, LONGSTR);
       if(nbytes <= 0){
         p->status = 0;
         continue;
       }
       buffer[nbytes] = 0;
       strcpy(p->ibuf, buffer);
     }
   }
}

void remove_stuff(int sock)
{
  NODE *p, *q;

  while(head && head->status == 0){
    p = head;
    head = head->link;
    free(p);
  }
  if(!head)
    return;
  p = head;
  while(p){
    q = p->link;   // i know p is not null
    if(q && q->status == ST_BAD){
      p->link = q->link;      // might be null
      free(q);
    } else {
      p = p->link;
    }
  }
}
int countusers()
{
  int n;
  NODE *p;

  n = 0;
  for(p = head ; p ; p = p->link)
    n++;
  return n;
}
void exit_nicely() {
  
  NODE *p;
  fprintf(stderr,"\nclosing descriptors...\n");
  for(p = head ; p ; p = p->link)
    close(p->desc);
  exit(0);
}
