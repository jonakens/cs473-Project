#include "server.h"

extern sqlite3 *db;
extern GROUP *head;

/*
  Setting up the priority for this program
*/
void set_priority()
{
  pid_t who = getpid(); //get process id
  int which = PRIO_PROCESS, priority = 19;
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
  struct sockaddr_in local_addr;
  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);

  int local_sock = socket(AF_INET, SOCK_STREAM, 0); //socket initialization
  if (local_sock < 0) {
    perror("[Socket initialization]");
    exit(1);
  }

  //bind socket with server information
  if (bind(local_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
    perror("[Socket binding]");
    close(local_sock);
    exit(1);
  }

  //listen up to 256 connections
  listen(local_sock, 256);
  return local_sock;
}

/*
  handle new connection request
*/
int new_connection(int local_sock)
{
  struct sockaddr remote_addr;
  int size = sizeof(remote_addr), remote_sock;
  if ((remote_sock = accept(local_sock, &remote_addr, &size)) < 0) {
    perror("[Accept]");
    return -1;
  }

  return remote_sock;
}

/*
  handle incoming connection request
  get client information
  set up a node to keep client information
*/
void open_stuff(int local_sock)
{
  fd_set input_set;
  FD_ZERO(&input_set);
  FD_SET(local_sock, &input_set);
  struct timeval timeout; //setting timeout value to 0, or do not wait
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  select(local_sock+1, &input_set, 0, 0, &timeout);
  if (!FD_ISSET(local_sock, &input_set)) return;

  //handle new connection request
  int remote_sock = new_connection(local_sock);
  if (remote_sock < 0) return;

  struct sockaddr_in remote_addr;
  int size = sizeof(remote_addr);
  //get client info
  if (getpeername(remote_sock, (struct sockaddr *)&remote_addr, &size) < 0) {
    perror("[Getpeername]");
    close(remote_sock);
    return;
  }

  GROUP *group;
  for (group = head; group != NULL; group = group->link) {
    if (strcmp(group->name, "general") == 0) break;
  }

  if (group == NULL) {
    group = make_group("general", NULL);
    head = group;
  }

  if (group->member == NULL) { //the general room has no member
    group->member = make_node(remote_sock);
  } else { //general room has member(s), then put new client in front
    NODE *member;
    member = make_node(remote_sock);
    member->link = group->member;
    group->member = member;
  }

  //filling in the client ip address to the user node
  (group->member)->addr = strdup(inet_ntoa(remote_addr.sin_addr));
  //printing greeting message
  print_help(group->member);
  //print initial prompt
  print_prompt(group, group->member);
}

/*
  flush message
  send the message to the client fd
*/
void write_stuff ()
{
  GROUP *group;
  NODE *user;
  fd_set output_set;
  FD_ZERO(&output_set);
  int max_desc = 0;

  //iterate the group, and iterate member of the group
  //to check if anyone has received message in its output buffer
  for (group = head; group != NULL; group = group->link) {
    for (user = group->member; user != NULL; user = user->link) {
      if (user->status == ST_BAD) continue;
      if (user->obuf != NULL) {
        FD_SET(user->desc, &output_set);
        if (user->desc > max_desc) max_desc = user->desc;
      }
    }
  }

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  select(max_desc+1, 0, &output_set, 0, &timeout);

  //iterate the group, then its member again to write out to remote end
  for (group = head; group != NULL; group = group->link) {
    for(user = group->member; user != NULL; user = user->link){
      if(FD_ISSET(user->desc, &output_set)){
        int len = strlen((user->obuf)->message);
        int bytes_written = write(user->desc, (user->obuf)->message, len); //read from the message head
        remove_obuf(user); //remove the message head

        if (bytes_written < len) {
          user->status = ST_BAD;
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
void read_stuff ()
{
  GROUP *group;
  NODE *user;
  fd_set input_set;
  FD_ZERO(&input_set);
  int max_desc = 0;

  //mark the all online node, to monitor input
  for (group = head; group != NULL; group = group->link) {
    for(user = group->member; user != NULL; user = user->link){
      FD_SET(user->desc, &input_set);
      if(user->desc > max_desc) max_desc = user->desc;
    }
  }

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  select(max_desc+1, &input_set, 0, 0, &timeout);

  int bytes_read;
  char buffer[LONGSTR];

  for (group = head; group != NULL; group = group->link) {
    for(user = group->member; user != NULL; user = user->link){
      if(FD_ISSET(user->desc, &input_set)){
        if (user->status == ST_BAD) continue;

        memset(buffer, 0, LONGSTR);
        //why 100? bcs we may need to concat user info later
        bytes_read = read(user->desc, buffer, LONGSTR-100); //read input from remote sock
        if(bytes_read <= 0){
          user->status = ST_BAD;
          continue;
        } else if (bytes_read >= LONGSTR-100) {
          memset(buffer, 0, LONGSTR); //clear up the buffer
          strcpy(buffer, "[Buffer overflow]\n"); //then fill in with error msg
          send_to_obuf(user, buffer);
          continue;
        }

        if (buffer[bytes_read-1] == '\n') { //check if line ends with newline
          buffer[bytes_read-1] = '\0'; //if it does, replace with null bybte
        } else {
          buffer[bytes_read] = '\0'; //otherwise just null terminate
        }

        //replace escpace seq (ascii 27)
        int i;
        for (i = 0; i < strlen(buffer); i++) {
          if (buffer[i] == 27) buffer[i] = ' ';
        }
        //send the message, even if it's empty
        //the reason is that we can know if user has deliberately
        //press enter to move cursor, thus we can print the prompt
        send_to_ibuf(user, buffer);
      }
    }
  }
}

/*
  user input processing
  help manage user command and text input
*/
void process_stuff ()
{
  GROUP *group;
  NODE *curr_user, *target_user;
  char buffer[LONGSTR];
  char msg[LONGSTR];

  for (group = head; group != NULL; group = group->link) {
    for(curr_user = group->member; curr_user != NULL; curr_user = curr_user->link) {
      if(curr_user->status == ST_BAD || curr_user->ibuf == NULL) continue;

      memset(buffer, 0, LONGSTR);
      memset(msg, 0, LONGSTR);
      strcpy(buffer, (curr_user->ibuf)->message); //retrieve the buffer from user ibuf
      remove_ibuf(curr_user); //remove the actual message node from the user node

      //handle when user presses enter deliberately
      //print prompt
      if (strlen(buffer) == 0) {
        print_prompt(group, curr_user);
        continue;
      }

      //parse the buffer to see if the user has written a special command
      //the command will call handler to execute the right
      //procedure, the lex_string function also fill in the
      //argument given in the string msg
      token_t tok;
      switch((tok = lex_string(buffer, msg))) {
        case T_STOP: //stop the connection to the server
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_HELP:
          //call the function to print help message
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_WHO:
          //get list of online users in the whole server
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_LOOK:
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_LOGOUT:
          //go from any chat room to the main menu state
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_LOGIN:
          //get the user into the general chat room
          command_handler(group, curr_user, tok, msg);
          continue;
        case T_REGISTER:
          //creating new user Account
          command_handler(group, curr_user, tok, msg);
          continue;
        case T_PASSWD:
          //change the user password
          command_handler(group, curr_user, tok, msg);
          continue;
        case T_PRIVATE:
          //send a private message to a users
          command_handler(group, curr_user, tok, msg);
          continue;
        case T_GROUP:
          //make a group if not already exist
          command_handler(group, curr_user, tok, msg);
          continue;
        case T_EXIT:
          //exit from a group and go to general room
          command_handler(group, curr_user, tok, "general");
          continue;
        case T_QUIZ:
          command_handler(group, curr_user, tok, NULL);
          continue;
        case T_OTHER:
          //no command detected
          switch (curr_user->status) {
            case ST_MENU:
              //unknown command
              sprintf(msg, "[Unknown command]\n");
              send_to_obuf(curr_user, msg);
              print_prompt(group, curr_user);
              continue;
            case ST_CHAT:
              //not a command, just a string
              //here the message is padded with the sender information
              sprintf(msg, "\n[Message] %s (%s) says %s\n", curr_user->name, curr_user->addr, buffer);
              //now distribute the message
              for(target_user = group->member; target_user != NULL; target_user = target_user->link) {
                if(target_user->status != ST_CHAT || target_user == curr_user) continue;
                send_to_obuf(target_user, msg);
                print_prompt(group, target_user);
              }
              print_prompt(group, curr_user);
              continue;
            case ST_ANSWER:
              validate_answer(group, curr_user, buffer);
              continue;
          }
      }
    }
  }
}

/*
  clear up node that is in ST_BAD
  freeing all the memory allocated
*/
void remove_stuff ()
{
  GROUP *group;
  NODE *curr_user, *next_user;

  for (group = head; group != NULL; group = group->link) {
    //check the head of the member for bad nodes
    while (group->member && (group->member)->status == ST_BAD) {
      curr_user = group->member;
      group->member = (group->member)->link;
      free(curr_user->name);
      free(curr_user->hash);
      free(curr_user->addr);
      free(curr_user->answer);
      free_ibuf(curr_user);
      free_obuf(curr_user);
      close(curr_user->desc);
      free(curr_user);
    }

    //find out if the head of member pointed to null
    //which means there is no more nodes
    if(!(group->member)) continue;

    //bad nodes at head has been terminated
    //check is there is bad node after the head
    curr_user = group->member;
    while (curr_user) {
      next_user = curr_user->link;
      if (next_user && next_user->status == ST_BAD) {
        curr_user->link = next_user->link;
        free(next_user->name);
        free(next_user->hash);
        free(next_user->addr);
        free(next_user->answer);
        free_ibuf(next_user);
        free_obuf(next_user);
        close(next_user->desc);
        free(next_user);
      } else {
        curr_user = curr_user->link;
      }
    }
  }
}

/*
  allocate a struct to contain client information
  as a part of linked list and initialize it
*/
NODE *make_node (int sock)
{
  NODE *user;
  user = malloc(sizeof(NODE));
  user->desc = sock;
  user->status = ST_MENU;
  user->name = NULL;
  user->hash = NULL;
  user->addr = NULL;
  user->answer = NULL;
  user->ibuf  = NULL;
  user->obuf = NULL;
  user->login_time = 0;
  user->link = NULL;
  return user;
}

/*
  allocate a message node as a part of linked list ibuf or obuf
*/
MESS *make_message ()
{
  MESS *msg;
  msg = malloc(sizeof(MESS));
  msg->message = malloc(LONGSTR*sizeof(char));
  msg->messlink = NULL;
  return msg;
}

/*
  allocate a group node as a part of linked list of groups
*/
GROUP *make_group (char group_name[], char group_key[])
{
  GROUP *group;
  group = malloc(sizeof(GROUP));
  group->name = strdup(group_name);
  group->member = NULL;
  group->link = NULL;

  if (group_key != NULL) {
    group->key = strdup(group_key); //a key to enter the room
  } else {
    group->key = NULL;
  }

  return group;
}

/*
  send message to the node input buffer
  input buffer is a temporary holder
  before the message is processed by the server
*/
void send_to_ibuf (NODE *target_user, char message[])
{
  if (target_user->ibuf == NULL) {
    target_user->ibuf = make_message();
    strcpy((target_user->ibuf)->message, message);
  } else {
    MESS *last_msg;
    //iterating the linked list before putting the message at the end of the list
    for (last_msg = target_user->ibuf; last_msg->messlink != NULL; last_msg = last_msg->messlink) {}

    MESS *new_msg;
    new_msg = make_message();
    strcpy(new_msg->message, message);
    last_msg->messlink = new_msg;
  }
}

/*
  send message to the node output buffer
  output buffer is a temporary holder
  before the message is flushed to the user
*/
void send_to_obuf (NODE *target_user, char message[])
{
  if (target_user->obuf == NULL) {
    target_user->obuf = make_message();
    strcpy((target_user->obuf)->message, message);
  } else {
    MESS *last_msg;
    //iterating the linked list before putting the message at the end of the list
    for (last_msg = target_user->obuf; last_msg->messlink != NULL; last_msg = last_msg->messlink) {}

    MESS *new_msg;
    new_msg = make_message();
    strcpy(new_msg->message, message);
    last_msg->messlink = new_msg;
  }
}

/*
  remove the head of input buffer
  used after message has been processed by the server
  or when freeing the allocation bcs user/client is
  in ST_BAD
*/
void remove_ibuf (NODE *user)
{
  MESS *msg_head;
  msg_head = user->ibuf;
  if (msg_head == NULL) {
    return;
  }

  MESS *next_msg;
  next_msg = msg_head->messlink;
  if (next_msg == NULL) {
    user->ibuf = NULL;
    free(msg_head->message);
    free(msg_head);
  } else {
    user->ibuf = next_msg;
    free(msg_head->message);
    free(msg_head);
  }
}

/*
  remove the head of output buffer
  used after message has been flushed to the users
  or when freeing the allocation bcs user/client is
  in ST_BAD
*/
void remove_obuf (NODE *user)
{
  MESS *msg_head;
  msg_head = user->obuf;
  if (msg_head == NULL) {
    return;
  }

  MESS *next_msg;
  next_msg = msg_head->messlink;
  if (next_msg == NULL) {
    user->obuf = NULL;
    free(msg_head->message);
    free(msg_head);
  } else {
    user->obuf = next_msg;
    free(msg_head->message);
    free(msg_head);
  }
}

/*
  clearing up the whole input buffer linked list
*/
void free_ibuf (NODE *user)
{
  while ((user->ibuf) != NULL) {
    remove_ibuf(user);
  }
}

/*
  clearing up the whole output buffer linked list
*/
void free_obuf (NODE *user)
{
  while ((user->obuf) != NULL) {
    remove_obuf(user);
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
  while (isspace(c)) c = line[++lp];

  //actually getting the first token
  while (!isspace(c) && c != '\0') {
    args[ap++] = c;
    c = line[++lp];
  }
  args[ap] = '\0';

  //check the first token for special commands
  token_t tok;
  switch ((tok = special_check(args))) {
    case T_STOP: return tok;
    case T_HELP: return tok;
    case T_WHO: return tok;
    case T_LOOK: return tok;
    case T_LOGOUT: return tok;
    case T_EXIT: return tok;
    case T_QUIZ: return tok;
    case T_OTHER: return tok;
    case T_LOGIN:
    case T_REGISTER:
    case T_PASSWD:
    case T_PRIVATE:
    case T_GROUP:
      //login, register, passwd, private, group
      //fall into the second phase to get the arguments
      //there could be many, so get until null, and parse
      //for the need of each command later. the argument
      //is kept in the variable passed to this function
      //called args
      ap = 0;
      memset(args, 0, LONGSTR);
      while (isspace(c)) c = line[++lp];

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
  else if (strcmp(token, "/quiz") == 0) return T_QUIZ;
  else return T_OTHER;
}

/*
  called when user pressed ctrl+c to kill the server
  the function will remove the group nodes and each
  of the member
*/
void exit_nicely ()
{
  fprintf(stderr, "\n[Closing Descriptors...]\n");

  while (head) {
    GROUP *group = head;
    head = head->link;

    while (group->member) {
      NODE *user = group->member;
      group->member = (group->member)->link;
      free(user->name);
      free(user->hash);
      free(user->addr);
      free(user->answer);
      free_ibuf(user);
      free_obuf(user);
      close(user->desc);
      free(user);
    }

    free(group->name);
    free(group->key);
    free(group);
  }

  fprintf(stderr, "[Closing Database...]\n");
  sqlite3_close(db);

  exit(0);
}

/*
  count number of user in the whole server
*/
int count_users ()
{
  int total = 0;
  GROUP *group;
  NODE *user;
  for (group = head; group != NULL; group = group->link) {
    for(user = group->member; user != NULL; user = user->link)
      total++;
  }
  return total;
}

/*
  print prompt based on user state (menu or chat)
	or the current group for user (general or anything else)
*/
void print_prompt (GROUP *group, NODE *user)
{
  char prompt[SHORTSTR];
  memset(prompt, 0, SHORTSTR);

  if (user->status == ST_MENU) {
    sprintf(prompt, "menu> ");
  } else {
    sprintf(prompt, "%s> ", group->name);
  }

  send_to_obuf(user, prompt);
}
