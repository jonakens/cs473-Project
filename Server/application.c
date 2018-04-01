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
  who = getpid();

  int ret = setpriority(which, who, priority);
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
  int ret = sqlite3_open("server.db", &db);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Database Open] %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  } else {
    fprintf(stderr, "[Database Open] Success\n");
  }
}

int init_socket(int port)
{
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("[Socket Initialization]");
    exit(1);
  }

  if(bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    perror("[Binding]");
    close(s);
    exit(1);
  }

  listen(s, 256);
  return s;
}

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

  int desc = new_connection(s);
  if(desc < 0)
    return;

  struct sockaddr_in sock;
  int size = sizeof(sock);
  if (getpeername(desc, (struct sockaddr *)&sock, &size) < 0) {
    perror("[Get Peer Name]");
    close(desc);
    return;
  }

  NODE *p;
  if(head == NULL){
    head = make_node(desc);
  } else {
    p = make_node(desc);
    p->link = head;
    head = p;
  }

  head->addr = strdup(inet_ntoa(sock.sin_addr));
  print_help(head);
}

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
  p->link = NULL;
  return p;
}

MESS *make_message ()
{
  MESS *ms;
  ms = malloc(sizeof(MESS));
  ms->message = malloc(LONGSTR*sizeof(char));
  ms->messlink = NULL;
  return ms;
}

void send_to_obuf (NODE *target, char message[])
{
  if (target->obuf == NULL) {
    target->obuf = make_message();
    strcpy((target->obuf)->message, message);
  } else {
    MESS *end;
    for (end = target->obuf; end != NULL; end = end->messlink) {}

    MESS *new;
    new = make_message();
    strcpy(new->message, message);
    new->messlink = end->messlink;
    end->messlink = new;
  }
}

void send_to_ibuf (NODE *target, char message[])
{
  if (target->ibuf == NULL) {
    target->ibuf = make_message();
    strcpy((target->ibuf)->message, message);
  } else {
    MESS *end;
    for (end = target->ibuf; end != NULL; end = end->messlink) {
    }
    MESS *new;
    new = make_message();
    strcpy(new->message, message);
    new->messlink = end->messlink;
    end->messlink = new;
  }
}

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

void free_ibuf (NODE *nd)
{
  while ((nd->ibuf) != NULL) {
    remove_ibuf(nd);
  }
}

void free_obuf (NODE *nd)
{
  while ((nd->obuf) != NULL) {
    remove_obuf(nd);
  }
}

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
  char msg[LONGSTR];
  memset(buffer, 0, LONGSTR);
  memset(msg, 0, LONGSTR);

  for(p = head; p != NULL; p = p->link){
    if(FD_ISSET(p->desc, &iset)){
      if (p->status == ST_BAD) continue;

      nbytes = read(p->desc, buffer, LONGSTR);
      if(nbytes <= 0){
        p->status = ST_BAD;
        continue;
      } else if (nbytes >= LONGSTR) {
        memset(buffer, 0, LONGSTR);
        memset(msg, 0, LONGSTR);
        strcpy(msg, "[BUFFER OVERFLOW]\n");
        send_to_obuf(p, msg);
        continue;
      } else {
        buffer[nbytes-1] = '\0';
      }

      if (strlen(buffer) == 0) {
        continue;
      } else {
        send_to_ibuf(p, buffer);
      }
    }
  }
}

void process_stuff (int sock)
{
  NODE *p, *q;
  char buffer[LONGSTR];
  char msg[LONGSTR];
  token_t tok;

  memset(buffer, 0, LONGSTR);
  memset(msg, 0, LONGSTR);

  for(p = head; p != NULL; p = p->link) {
    if(p->status == ST_BAD || p->ibuf == NULL) continue;

    memset(buffer, 0, LONGSTR);
    strcpy(buffer, (p->ibuf)->message);
    remove_ibuf(p);
    switch((tok = lex_string(buffer))) {
      case T_QUIT:
        command_handler(p, tok);
        continue;
      case T_HELP:
        command_handler(p, tok);
        continue;
      case T_LOGIN:
        command_handler(p, tok);
        continue;
      case T_LOGOUT:
        command_handler(p, tok);
        continue;
      case T_NEW:
        command_handler(p, tok);
        continue;
      case T_LIST:
        command_handler(p, tok);
        continue;
      case T_OTHER:
        break;
    }

    switch (p->status) {
      case ST_MENU:
        memset(msg, 0, LONGSTR);
        sprintf(msg, "[Unknown Command]\n");
        send_to_obuf(p, msg);
        continue;
      case ST_NEWUSR:
        check_username(p->status, p, buffer);
        continue;
      case ST_NEWPWD:
        check_password(p->status, p, buffer);
        continue;
      case ST_LOGIN:
        check_username(p->status, p, buffer);
        continue;
      case ST_PASSWD:
        check_password(p->status, p, buffer);
        continue;
      case ST_CHAT:
        memset(msg, 0, LONGSTR);
        sprintf(msg, "\n%s (%s): %s\n", p->name, p->addr, buffer);
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
      free_ibuf(q);
      free_obuf(q);
      close(q->desc);
      free(q);
    } else {
      p = p->link;
    }
  }
}

token_t lex_string (char line[])
{
  int pos = 0, idx = 0;
  char c = line[pos];
  char token[LONGSTR];
  memset(token, 0, LONGSTR);

  while (c != '\0') {
    if (c == ' ' || c == '\t') {
      if (strcmp(token, "") == 0) {
        c = line[++pos];
        continue;
      } else {
        return special_check(token);
      }
    }
    token[idx++] = line[pos];
    c = line[++pos];
  }

  return special_check(token);
}

token_t special_check (char token[])
{
  if (strcmp(token, "/quit") == 0) return T_QUIT;
  else if (strcmp(token, "/help") == 0) return T_HELP;
  else if (strcmp(token, "/login") == 0) return T_LOGIN;
  else if (strcmp(token, "/logout") == 0) return T_LOGOUT;
  else if (strcmp(token, "/new") == 0) return T_NEW;
  else if (strcmp(token, "/list") == 0) return T_LIST;
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
