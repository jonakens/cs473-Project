#include "server.h"

extern sqlite3 *db;
extern GROUP *head;

/*
  Setting up the priority for this program
*/
void set_priority()
{
  pid_t who = getpid(); //get process id
  int priority = 19, which = PRIO_PROCESS;
  int ret = setpriority(which, who, priority); //set the process priority

  if (ret < 0) {
    fprintf(stderr, "[Failed to set priority]\n");
    exit(1);
  }

  fprintf(stderr, "[Priority] %d\n", getpriority(which, who));
}

/*
  initialize database connection.
  the server will not run without it, so it won't work if failed to open
*/
void init_database ()
{
  char dbfile[] = "server.db";
  int ret = sqlite3_open(dbfile, &db); //open sqlite3 database
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Failed to open database] %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  } else {
    fprintf(stderr, "[Database] %s\n", dbfile);
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
    perror("[Socket initialization]");
    exit(1);
  }

  //bind socket with server information
  if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    perror("[Socket binding]");
    close(s);
    exit(1);
  }

  //listen up to 256 connections
  listen(s, 256);
  return s;
}

/*
  handle new connection request
*/
int new_connection(int s)
{
  struct sockaddr isa;
  int size = sizeof(isa), t;
  getsockname(s, &isa, &size);

  if ((t = accept(s, &isa, &size)) < 0) {
    perror("[Accept]");
    return -1;
  }
  return t;
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
  if (!FD_ISSET(s, &iset)) return;

  //handle new connection request
  int desc = new_connection(s);
  if (desc < 0) return;

  struct sockaddr_in sock;
  int size = sizeof(sock);
  //get client info
  if (getpeername(desc, (struct sockaddr *)&sock, &size) < 0) {
    perror("[Getpeername]");
    close(desc);
    return;
  }

  //check if the general chat room has been established
  if (head == NULL) {
    head = make_group("general"); //set up general chat room
    head->memlist = make_node(desc);
  } else { //general room exist, then put new client in
    if (head->memlist == NULL) { //the general room has no member
      head->memlist = make_node(desc);
    } else { //general room has member(s), then put new client in front
      NODE *member;
      member = make_node(desc);
      member->link = head->memlist;
      head->memlist = member;
    }
  }

  //filling in the client ip address to the user node
  (head->memlist)->addr = strdup(inet_ntoa(sock.sin_addr));
  //printing greeting message
  print_help(head->memlist);
  print_prompt(head, head->memlist);
}

/*
  flush message
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
  GROUP *g;
  NODE *p;

  //iterate the group, and iterate member of the group
  //to check if anyone has received message in its ouput buffer
  for (g = head; g != NULL; g = g->link) {
    for (p = g->memlist; p != NULL; p = p->link) {
      if (p->status == ST_BAD)
        continue;
      if (p->obuf != NULL) {
        FD_SET(p->desc, &oset);
        if (p->desc > dmax)
          dmax = p->desc;
      }
    }
  }

  select(dmax+1, 0, &oset, 0, &zero);

  //iterete the group, then its member again
  //while checking if the descriptors has been noted
  //as ready to flush to the client
  for (g = head; g != NULL; g = g->link) {
    for(p = g->memlist; p != NULL ; p = p->link){
      if(FD_ISSET(p->desc, &oset)){
        int len = strlen((p->obuf)->message); //read from the message head
        int rv = write(p->desc, (p->obuf)->message, len);
        remove_obuf(p); //remove the message head

        if (rv < len) {
          p->status = ST_BAD;
          continue;
        }
      }
    }
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
  GROUP *g;
  NODE *p;

  //check if any node has written sth
  for (g = head; g != NULL; g = g->link) {
    for(p = g->memlist; p != NULL ; p = p->link){
      FD_SET(p->desc, &iset);
      if(p->desc > dmax)
        dmax = p->desc;
    }
  }

  select(dmax+1, &iset, 0, 0, &zero);

  int nbytes;
  char buffer[LONGSTR];
  memset(buffer, 0, LONGSTR);

  for (g = head; g != NULL; g = g->link) {
    for(p = g->memlist; p != NULL; p = p->link){
      if(FD_ISSET(p->desc, &iset)){ //found a node that has written sth
        if (p->status == ST_BAD) continue;

        nbytes = read(p->desc, buffer, LONGSTR); //now, actually read it
        if(nbytes <= 0){
          p->status = ST_BAD;
          continue;
        } else if (nbytes >= LONGSTR-100) { //check if buffer overflow
          memset(buffer, 0, LONGSTR); //why 100? bcs we may need to concat user info later
          strcpy(buffer, "[Buffer overflow]\n");
          send_to_obuf(p, buffer);
          continue;
        }

        if (buffer[nbytes-1] == '\n') { //check if line ends with newline
          buffer[nbytes-1] = '\0'; //if it does, replace with null bybte
        } else {
          buffer[nbytes] = '\0'; //otherwise just null terminate
        }
        //replace esc seq
        int i;
        for (i = 0; i < strlen(buffer); i++) {
          if (buffer[i] == 27) buffer[i] = ' ';
        }
        //send the message, even if it's empty
        //the reason is that we can know if user has deliberately
        //press enter to move cursor, thus we can print the prompt
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
  GROUP *g;
  NODE *p, *q;
  char buffer[LONGSTR];
  char msg[LONGSTR];
  token_t tok;

  for (g = head; g != NULL; g = g->link) {
    for(p = g->memlist; p != NULL; p = p->link) {
      if(p->status == ST_BAD || p->ibuf == NULL) continue;

      memset(buffer, 0, LONGSTR);
      memset(msg, 0, LONGSTR);
      strcpy(buffer, (p->ibuf)->message); //retrieve the buffer from user ibuf
      remove_ibuf(p); //remove the actual message node from the user node

      //handle user presses enter deliberately
      //print prompt
      if (strlen(buffer) == 0) {
        print_prompt(g, p);
        continue;
      }

      //parse the buffer to see if the user has written a special command
      //the command will call handler to execute the right
      //procedure, if no argument needed, null is passed
      switch((tok = lex_string(buffer, msg))) {
        case T_STOP: //stop the connection to the server
          command_handler(NULL, p, tok, NULL);
          continue;
        case T_HELP:
          //call the function to print help message
          command_handler(g, p, tok, NULL);
          continue;
        case T_WHO:
          //get list of online users in the whole server
          command_handler(g, p, tok, NULL);
          continue;
				case T_LOOK:
					command_handler(g, p, tok, NULL);
					continue;
        case T_LOGOUT:
          //go from any chat room to the main menu state
          command_handler(g, p, tok, NULL);
          continue;
        case T_LOGIN:
          //get the user into the general chat room
          command_handler(g, p, tok, msg);
          continue;
        case T_REGISTER:
          //creating new user Account
					command_handler(g, p, tok, msg);
          continue;
        case T_PASSWD:
          //change the user password
          command_handler(g, p, tok, msg);
          continue;
        case T_PRIVATE:
          //send a private message to a users
          command_handler(g, p, tok, msg);
          continue;
        case T_GROUP:
          //make a group if not already exist
          command_handler(g, p, tok, msg);
          continue;
        case T_EXIT:
          //exit from a group and go to general room
          command_handler(g, p, tok, msg);
          continue;
        case T_OTHER:
          //no special token detected
          //included when the process return after chopping the user
          //command arguments
          break;
      }

      switch (p->status) {
        case ST_MENU:
          //no special token detected while in main menu
          //which means the command is unknown to the server
          memset(buffer, 0, LONGSTR);
          sprintf(buffer, "[Unknown command]\n");
          send_to_obuf(p, buffer);
          print_prompt(g, p);
          continue;
        case ST_LOGIN:
          //user is in login state, and has argument (user credentials)
          //to check to get into the general room
          check_credentials(p->status, p, buffer);
          print_prompt(g, p);
          continue;
        case ST_REGISTER:
          //user is in register state, and has argument (new user credentials)
          //to check to make new account and get into the general room
          check_credentials(p->status, p, buffer);
          print_prompt(g, p);
          continue;
        case ST_PRIVATE:
          //user is in private state, and has argument (username and message)
          //to be sent to a particular user
          private_message(p, buffer);
          print_prompt(g, p);
          continue;
        case ST_PASSWD:
          //user is in passwd state, and has argument (username, oldpass, and newpass)
          //to be check and replaced
          change_password(p, buffer);
          print_prompt(g, p);
          continue;
        case ST_GROUP:
          //user is in group state, and has argument (groupname)
          //to be used to go to different room
          print_prompt(manage_group(g, p, buffer), p);
          continue;
        case ST_EXIT:
          //user is in exit state, and has argument (groupname = general)
          //to be used to go to general room
          print_prompt(manage_group(g, p, buffer), p);
          continue;
        case ST_CHAT:
          //user is in chat state, and has new message for his groups
          //the message will only be sent to that group
          memset(msg, 0, LONGSTR);
          //here the message is padded with the sender information
          sprintf(msg, "\n[%s: %s] %s\n", p->name, p->addr, buffer);
          for(q = g->memlist; q != NULL; q = q->link) {
            if(q->status != ST_CHAT || p == q) continue;
            send_to_obuf(q, msg);
						print_prompt(g, q);
          }
          print_prompt(g, p);
          continue;
      }
    }
  }
}

/*
  clear up node that is in ST_BAD
  freeing all the memory allocated
*/
void remove_stuff (int sock)
{
  GROUP *g;
  NODE *p, *q;
  for (g = head; g != NULL; g = g->link) {
    //check the head of the memlist for bad nodes
    while (g->memlist && (g->memlist)->status == ST_BAD) {
      p = g->memlist;
      g->memlist = (g->memlist)->link;
      free(p->name);
      free(p->hash);
      free(p->addr);
      free_ibuf(p);
      free_obuf(p);
      close(p->desc);
      free(p);
    }

    //find out if the memlist pointed to null
    //which means there is no more nodes
    if(!(g->memlist))
      continue;

    //bad nodes at head has been terminated
    //check is there is bad node after the head
    p = g->memlist;
    while (p) {
      q = p->link;
      if (q && q->status == ST_BAD) {
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
}

/*
  allocate a struct to contain client information
  as a part of linked list and initialize it
*/
NODE *make_node (int desc)
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
  allocate a group node as a part of linked list of groups
*/
GROUP *make_group (char groupname[])
{
  GROUP *g;
  g = malloc(sizeof(GROUP));
  g->name = strdup(groupname);
  g->memlist = NULL;
  g->link = NULL;
  return g;
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
  lex_string is a function to parse the command and its argument
*/
token_t lex_string (char line[], char args[])
{
  int lp = 0, ap = 0;
  char c = line[lp];

  //getting rid of whitespaces before first token
  while (c == ' ' || c == '\t') c = line[++lp];

  //actually getting the first token
  while (c != '\0' && c != ' ' && c != '\t') {
    args[ap++] = c;
    c = line[++lp];
  }

  args[ap] = '\0';
  token_t tok;

  //check the first token for special commands
  switch ((tok = special_check(args))) {
    case T_OTHER: return tok;
    case T_STOP: return tok;
    case T_HELP: return tok;
    case T_WHO: return tok;
		case T_LOOK: return tok;
    case T_EXIT: return tok;
    case T_LOGOUT: return tok;
    case T_LOGIN:
    case T_REGISTER:
    case T_PASSWD:
    case T_PRIVATE:
    case T_GROUP:
      //logout, login, register, passed, private, group
      //fall into the second phase to get the arguments
      //there could be many, so get until null, and parse
      //for the need of each command later. the argument
      //is kept in the variable passed to this function
      //called args
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

/*
  check if the command from lex_string means sth special
*/
token_t special_check (char token[])
{
  //check for the possible commands
  if (strcmp(token, "/stop") == 0) return T_STOP;
  else if (strcmp(token, "/help") == 0) return T_HELP;
  else if (strcmp(token, "/who") == 0) return T_WHO;
	else if (strcmp(token, "/look") == 0) return T_LOOK;
  else if (strcmp(token, "/logout") == 0) return T_LOGOUT;
  else if (strcmp(token, "/login") == 0) return T_LOGIN;
  else if (strcmp(token, "/register") == 0) return T_REGISTER;
  else if (strcmp(token, "/passwd") == 0) return T_PASSWD;
  else if (strcmp(token, "/private") == 0) return T_PRIVATE;
  else if (strcmp(token, "/group") == 0) return T_GROUP;
  else if (strcmp(token, "/exit") == 0) return T_EXIT;
  else return T_OTHER;
}

/*
  called when user pressed ctrl+c to kill the server
  the function will remove the group nodes and each
  of the member
*/
void exit_nicely ()
{
  fprintf(stdout, "\n[Closing Descriptors...]\n");

  while (head) {
    GROUP *g = head;
    head = head->link;

    while (g->memlist) {
      NODE *p = g->memlist;
      g->memlist = (g->memlist)->link;
      free(p->name);
      free(p->hash);
      free(p->addr);
      free_ibuf(p);
      free_obuf(p);
      close(p->desc);
      free(p);
    }

    free(g->name);
    free(g);
  }

  fprintf(stdout, "[Closing Database...]\n");
  sqlite3_close(db);

  exit(0);
}

/*
  count number of user in the whole server
*/
int count_users ()
{
  int n = 0;
  GROUP *g;
  NODE *p;
  for (g = head; g != NULL; g = g->link) {
    for(p = g->memlist; p != NULL; p = p->link)
      n++;
  }
  return n;
}

/*
  print prompt based on user state (menu or chat)
	or the current group for user (general or anything else)
*/
void print_prompt (GROUP *grp, NODE *usr)
{
  char prompt[SHORTSTR];
  memset(prompt, 0, SHORTSTR);

  if (usr->status == ST_MENU) {
    sprintf(prompt, "menu> ");
  } else {
    sprintf(prompt, "%s> ", grp->name);
  }

  send_to_obuf(usr, prompt);
}
