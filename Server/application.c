#include "server.h"

extern NODE *head;
extern sqlite3 *db;

/*
  Setting up the priority for this program
*/
void set_priority()
{
  int priority, which;
  which = PRIO_PROCESS;
  priority = 19;

  pid_t who;
  who = getpid(); //get process id

  int ret = setpriority(which, who, priority); //set the process priority
  if(ret < 0){
    fprintf(stderr, "[Priority] Priority not set successfuly");
    exit(1);
  }
  fprintf(stderr, "[Priority] Priority was set to %d sucessfully.\n",getpriority(which, who));
}

/*
  initialize database connection.
  the server will not run without it, so it won't work if failed to open
*/
void init_database ()
{
  int ret = sqlite3_open("server.db", &db); //open sqlite3 database
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Database Open] %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  } else {
    fprintf(stderr, "[Database Open] Success\n");
  }
}

/*
  initialize socket for communication, binds socket with the server address
  and listen for connection
*/
int init_socket(int port)
{
  //clearing up the struct, then fill in the info about the server
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  int s = socket(AF_INET, SOCK_STREAM, 0); //socket initialization
  if (s < 0) {
    perror("[Socket Initialization]");
    exit(1);
  }

  //bind socket with server information
  if(bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    perror("[Binding]");
    close(s);
    exit(1);
  }

  //listen up to 256 connections
  listen(s, 256);
  return s;
}

/*
  handle incoming connection request
  get client information
  set up a node to keep client information
*/
void open_stuff(int s)
{
  struct timeval zero;
  zero.tv_sec = 0;
  zero.tv_usec = 0;

  fd_set iset;
  FD_ZERO(&iset);
  FD_SET(s, &iset);
  select(s+1, &iset, 0, 0, &zero);

  if(!FD_ISSET(s,&iset))
    return;

  //handle new connection request
  int desc = new_connection(s);
  if(desc < 0)
    return;

  struct sockaddr_in sock;
  int size = sizeof(sock);
  //get client info
  if (getpeername(desc, (struct sockaddr *)&sock, &size) < 0) {
    perror("[Get Peer Name]");
    close(desc);
    return;
  }

  //set up a node (part of linked list) to keep client info
  NODE *p;
  if(head == NULL){
    head = make_node(desc);
  } else {
    p = make_node(desc);
    p->link = head;
    head = p;
  }

  //fill in client ip info to the node
  head->addr = strdup(inet_ntoa(sock.sin_addr));
  print_help(head);
}

/*
  handle new connection request
*/
int new_connection(int s)
{
  struct sockaddr isa;
  int size, t;

  size = sizeof(isa);
  getsockname(s, &isa, &size);
  if((t = accept(s, &isa, &size)) < 0){
    perror("[Accept]");
    return -1;
  }
  return t;
}

/*
  allocate a struct to contain client information
  as a part of linked list and initialize it
*/
NODE *make_node(int desc)
{
  NODE *p;
  p = malloc(sizeof(NODE));
  p->desc = desc;
  p->status = ST_MENU;
  p->name = NULL;
  p->hash = NULL;
  p->addr = NULL;
  p->ibuf  = NULL;
  p->obuf = NULL;
  p->login_time = 0;
  p->link = NULL;
  return p;
}

/*
  allocate a message node as a part of linked list ibuf or obuf
*/
MESS *make_message ()
{
  MESS *ms;
  ms = malloc(sizeof(MESS));
  ms->message = malloc(LONGSTR*sizeof(char));
  ms->messlink = NULL;
  return ms;
}

/*
  send message to the node output buffer
  output buffer is a temporary holder
  before the message is flushed to the user
*/
void send_to_obuf (NODE *target, char message[])
{
  if (target->obuf == NULL) {
    target->obuf = make_message();
    strcpy((target->obuf)->message, message);
  } else {
    MESS *end;
    //iterating the linked list before putting the message at the end of the list
    for (end = target->obuf; end->messlink != NULL; end = end->messlink) {}

    MESS *new;
    new = make_message();
    strcpy(new->message, message);
    new->messlink = end->messlink;
    end->messlink = new;
  }
}

/*
  send message to the node input buffer
  input buffer is a temporary holder
  before the message is processed by the server
*/
void send_to_ibuf (NODE *target, char message[])
{
  if (target->ibuf == NULL) {
    target->ibuf = make_message();
    strcpy((target->ibuf)->message, message);
  } else {
    MESS *end;
    //iterating the linked list before putting the message at the end of the list
    for (end = target->ibuf; end->messlink != NULL; end = end->messlink) {}

    MESS *new;
    new = make_message();
    strcpy(new->message, message);
    new->messlink = end->messlink;
    end->messlink = new;
  }
}

/*
  message flusher
  send the message to the client fd
*/
void write_stuff (int sock)
{
  struct timeval zero;
  zero.tv_sec = 0;
  zero.tv_usec = 0;

  fd_set oset;
  FD_ZERO(&oset);

  int dmax = 0;
  NODE *p;
  for (p = head; p != NULL; p = p->link) {
    if (p->status == ST_BAD)
      continue;
    if (p->obuf != NULL) {
      FD_SET(p->desc, &oset);
      if (p->desc > dmax)
        dmax = p->desc;
    }
  }
  select(dmax+1, 0, &oset, 0, &zero);

  for(p = head; p != NULL ; p = p->link){
    if(FD_ISSET(p->desc, &oset)){
      int len = strlen((p->obuf)->message);
      int rv = write(p->desc, (p->obuf)->message, len);

      if (rv < len) {
        p->status = ST_BAD;
        return;
      }

      remove_obuf(p);
    }
  }
}

/*
  remove the head of input buffer
  used after message has been processed by the server
  or when freeing the allocation bcs user/client is
  in ST_BAD
*/
void remove_ibuf (NODE *nd)
{
  MESS *msp;
  msp = nd->ibuf;
  if (msp == NULL) {
    return;
  }

  MESS *next;
  next = msp->messlink;
  if (next == NULL) {
    nd->ibuf = NULL;
    free(msp->message);
    free(msp);
  } else {
    nd->ibuf = next;
    free(msp->message);
    free(msp);
  }
}

/*
  remove the head of output buffer
  used after message has been flushed to the users
  or when freeing the allocation bcs user/client is
  in ST_BAD
*/
void remove_obuf (NODE *nd)
{
  MESS *msp;
  msp = nd->obuf;
  if (msp == NULL) {
    return;
  }

  MESS *next;
  next = msp->messlink;
  if (next == NULL) {
    nd->obuf = NULL;
    free(msp->message);
    free(msp);
  } else {
    nd->obuf = next;
    free(msp->message);
    free(msp);
  }
}

/*
  clearing up the whole input buffer linked list
*/
void free_ibuf (NODE *nd)
{
  while ((nd->ibuf) != NULL) {
    remove_ibuf(nd);
  }
}

/*
  clearing up the whole output buffer linked list
*/
void free_obuf (NODE *nd)
{
  while ((nd->obuf) != NULL) {
    remove_obuf(nd);
  }
}

/*
  reading user input and error checking of the user has overflown the buffer
  move user input into the input buffer ibuf for processing
*/
void read_stuff (int sock)
{
  struct timeval zero;
  zero.tv_sec = 0;
  zero.tv_usec = 0;
  fd_set iset;
  FD_ZERO(&iset);

  int dmax = 0;
  NODE *p;
  for(p = head ; p != NULL ; p = p->link){
    FD_SET(p->desc, &iset);
    if(p->desc > dmax)
      dmax = p->desc;
  }
  select(dmax+1, &iset, 0, 0, &zero);

  int nbytes;
  char buffer[LONGSTR];
  memset(buffer, 0, LONGSTR);

  for(p = head; p != NULL; p = p->link){
    if(FD_ISSET(p->desc, &iset)){
      if (p->status == ST_BAD) continue;

      nbytes = read(p->desc, buffer, LONGSTR);
      if(nbytes <= 0){
        p->status = ST_BAD;
        continue;
      } else if (nbytes >= LONGSTR-100) {        //check if buffer overflow
        memset(buffer, 0, LONGSTR);              //why 100? bcs we may need to concat user info
        strcpy(buffer, "[BUFFER OVERFLOW]\n");
        send_to_obuf(p, buffer);
        continue;
      }

      if (buffer[nbytes-1] == '\n') {
        buffer[nbytes-1] = '\0';
      } else {
        buffer[nbytes] = '\0';
      }

      if (strlen(buffer) == 0) {
        continue;
      } else {
        send_to_ibuf(p, buffer);
      }
    }
  }
}

/*
  user input processing
  help manage user command and text input
*/
void process_stuff (int sock)
{
  NODE *p, *q;
  char buffer[LONGSTR];
  char msg[LONGSTR];
  token_t tok;

  for(p = head; p != NULL; p = p->link) {
    if(p->status == ST_BAD || p->ibuf == NULL) continue;

    memset(buffer, 0, LONGSTR);
    memset(msg, 0, LONGSTR);
    strcpy(buffer, (p->ibuf)->message);
    remove_ibuf(p);
    switch((tok = lex_string(buffer, msg))) {
      case T_QUIT:
        command_handler(p, tok, NULL);
        continue;
      case T_HELP:
        command_handler(p, tok, NULL);
        continue;
      case T_WHO:
        command_handler(p, tok, NULL);
        continue;
      case T_LOGOUT:
        command_handler(p, tok, NULL);
        continue;
      case T_LOGIN:
        if (strlen(msg) > 0) {
          command_handler(p, tok, msg);
        } else {
          memset(msg, 0, LONGSTR);
          sprintf(msg, "[No argument]\n");
          send_to_obuf(p, msg);
        }
        continue;
      case T_REGISTER:
        if (strlen(msg) > 0) {
          command_handler(p, tok, msg);
        } else {
          memset(msg, 0, LONGSTR);
          sprintf(msg, "[No argument]\n");
          send_to_obuf(p, msg);
        }
        continue;
      case T_PRIVATE:
        if (strlen(msg) > 0) {
          command_handler(p, tok, msg);
        } else {
          memset(msg, 0, LONGSTR);
          sprintf(msg, "[No argument]\n");
          send_to_obuf(p, msg);
        }
        continue;
      case T_OTHER:
        break;
    }

    switch (p->status) {
      case ST_MENU:
        memset(buffer, 0, LONGSTR);
        sprintf(buffer, "[Unknown Command]\n");
        send_to_obuf(p, buffer);
        continue;
      case ST_LOGIN:
        check_credentials(p->status, p, buffer);
        continue;
      case ST_REGISTER:
        check_credentials(p->status, p, buffer);
        continue;
      case ST_PRIVATE:
        private_message(p, buffer);
        continue;
      case ST_CHAT:
        memset(msg, 0, LONGSTR);
        sprintf(msg, "\n[%s:%s] %s\n", p->name, p->addr, buffer);
        for(q = head ; q != NULL ; q = q->link) {
          if(q->status != ST_CHAT || p == q) continue;
          send_to_obuf(q, msg);
        }
        continue;
    }
  }
}

void remove_stuff (int sock)
{
  NODE *p, *q;
  while(head && head->status == ST_BAD){
    p = head;
    head = head->link;
    free(p->name);
    free(p->hash);
    free(p->addr);
    free_ibuf(p);
    free_obuf(p);
    close(p->desc);
    free(p);
  }
  if(!head)
    return;

  p = head;
  while(p){
    q = p->link;
    if(q && q->status == ST_BAD){
      p->link = q->link;
      free(q->name);
      free(q->hash);
      free(q->addr);
      free_ibuf(q);
      free_obuf(q);
      close(q->desc);
      free(q);
    } else {
      p = p->link;
    }
  }
}

token_t lex_string (char line[], char args[])
{
  int lp = 0, ap = 0;
  char c = line[lp];

  while (c == ' ' || c == '\t') c = line[++lp];

  while (c != '\0' && c != ' ' && c != '\t') {
    args[ap++] = c;
    c = line[++lp];
  }

  args[ap] = '\0';
  token_t tok;

  switch ((tok = special_check(args))) {
    case T_OTHER: return tok;
    case T_QUIT: return tok;
    case T_HELP: return tok;
    case T_WHO: return tok;
    case T_LOGOUT: return tok;
    case T_LOGIN:
    case T_REGISTER:
    case T_PRIVATE:
      ap = 0;
      memset(args, 0, LONGSTR);
      while (c == ' ' || c == '\t') c = line[++lp];

      while (c != '\0') {
        args[ap++] = c;
        c = line[++lp];
      }

      args[ap] = '\0';
      return tok;
  }
}

token_t special_check (char token[])
{
  if (strcmp(token, "/quit") == 0) return T_QUIT;
  else if (strcmp(token, "/help") == 0) return T_HELP;
  else if (strcmp(token, "/who") == 0) return T_WHO;
  else if (strcmp(token, "/logout") == 0) return T_LOGOUT;
  else if (strcmp(token, "/login") == 0) return T_LOGIN;
  else if (strcmp(token, "/register") == 0) return T_REGISTER;
  else if (strcmp(token, "/private") == 0) return T_PRIVATE;
  else return T_OTHER;
}

void exit_nicely ()
{
  NODE *p;
  fprintf(stdout, "\n[Closing Descriptors...]\n");

  for (p = head; p; p = p->link) {
    close(p->desc);
  }

  fprintf(stdout, "[Closing Database...]\n");
  sqlite3_close(db);

  exit(0);
}

int count_users ()
{
  int n = 0;
  NODE *p;
  for(p = head ; p ; p = p->link)
    n++;
  return n;
}
