#include "server.h"

extern GROUP *head;

/*
  handle all the execution of commands
*/
void command_handler(NODE *user, token_t cmd, char *args)
{
  char msg[LONGSTR];
  memset(msg, 0, LONGSTR);

  switch (cmd) {
    case T_STOP:
      user->status = ST_BAD; //change the status, so that it can be reaped by the remove_stuff function later
      return;
    case T_HELP:
      print_help(user); //call the function to print help message
      return;
    case T_WHO:
      //get list of online users
      //command only work for online (logged in) users
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }
      who_is_online(user);
      return;
    case T_LOGOUT:
      //go from any chat room to the main menu state
      //command only work for online (logged in) users
      if (user->status == ST_CHAT) {
        user->status = ST_MENU;
      } else {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      }
      return;
    case T_LOGIN:
      //get the user into the general chat room
      //cannot login when already logged in
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_LOGIN
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Already logged in, please first logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_LOGIN;
        send_to_ibuf(user, args);
      }
      return;
    case T_REGISTER:
      //creating new user Account
      //cannot make one when already login
      //has to be in the menu ST_MENU state to make one
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_REGISTER
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_REGISTER;
        send_to_ibuf(user, args);
      }
      return;
    case T_PASSWD:
      //change the user password
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_PASSWD
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }

      user->status = ST_PASSWD;
      send_to_ibuf(user, args);
      return;
    case T_PRIVATE:
      //send a private message to a users
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_PRIVATE
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }

      user->status = ST_PRIVATE;
      send_to_ibuf(user, args);
      return;
    case T_GROUP:
      //make a group if not already exist
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_GROUP
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }

      user->status = ST_GROUP;
      send_to_ibuf(user, args);
      return;
    case T_EXIT:
      //exit from a group and go to general room
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_EXIT
      //it is a default command as the user exits from any group
      //to the general chat room
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }

      user->status = ST_EXIT;
      send_to_ibuf(user, args);
      return;
  }
}

/*
  function to print help information
*/
void print_help(NODE *user)
{
  char help_message[K] = "You can use the following commands:\n\
  /login    <username> <password>                        Enter the general chat room.\n\
  /logout                                                Logout and go to main menu.\n\
  /register <username> <password>                        Make a new account and login.\n\
  /passwd   <username> <current password> <new password> Change the password for <username>.\n\
  /private  <username> <message>                         Send a private message to <username>.\n\
  /group    <groupname>                                  Move to a different room.\n\
  /exit                                                  Exit a chat room and to general chat room.\n\
  /who                                                   List online user.\n\
  /help                                                  Print this message.\n\
  /stop                                                  Close the connection to the chat server.\n";

  send_to_obuf(user, help_message);
}

/*
  finding out who are online
*/
void who_is_online (NODE *user)
{
  GROUP *g;
  NODE *p;
  char list[LONGSTR];
  char person[K];

  memset(list, 0, LONGSTR);

  for (g = head; g != NULL; g = g->link) {
    for (p = g->memlist; p != NULL; p = p->link) {
      if (p->status == ST_CHAT) {
        memset(person, 0, K);
        sprintf(person, "[%s: %s]\n", p->name, p->addr);
        strcat(list,person);
      }
    }
  }

  send_to_obuf(user, list);
}

/*
  sending private message to particular user
  there could multiple user logged in with the
  same username
*/
void private_message (NODE *user, char argument[])
{
  char username[K], message[K], line[LONGSTR];
  parse_args(user->status, argument, username, message, NULL);
  user->status = ST_CHAT;
  int flag = 0;

  GROUP *g;
  NODE *p;
  for (g = head; g != NULL; g = g->link) {
    for (p = g->memlist; p != NULL; p = p->link) {
      if (p->status != ST_CHAT) continue;
      if (strcmp(p->name, username) == 0) {
        flag = 1; //report user found
        sprintf(line, "[Private message from %s] %s\n", user->name, message);
        send_to_obuf(p, line);
      }
    }
  }

  memset(line, 0, LONGSTR);
  if (flag == 0) {
    sprintf(line, "[Receiver offline]\n");
    send_to_obuf(user, line);
  } else {
    sprintf(line, "[Message sent]\n");
    send_to_obuf(user, line);
  }
}

/*
  move a user node from one group into another
*/
GROUP *manage_group (GROUP *gnode, NODE *user, char argument[])
{
  char groupname[K], garbage[K];
  //only need first argument, but still need to provide variable for
  //second argument (empty string)
  parse_args(user->status, argument, groupname, garbage, NULL);

  if (check_length(user, groupname, "group name") == 0) {
    user->status = ST_CHAT;
    return gnode;
  }

  if (check_combination(user, groupname, "group name") == 0) {
    user->status = ST_CHAT;
    return gnode;
  }

  //looking for the target group to move the user
  //by checking the name of the group, if found
  //put it in the target_group var
  GROUP *target_group = NULL, *g;
  for (g = head; g != NULL; g = g->link) {
    if (strcmp(g->name, groupname) == 0) {
      target_group = g;
      break;
    }
  }

  //check if target has been found, else make new one
  //not found when NULL, thus make new one and put it
  //at the end of the group list
  if (target_group == NULL) {
    target_group = make_group(groupname);
    for (g = head; g->link != NULL; g = g->link) {}
    g->link = target_group;
  }

  //take off the user node from the source group
  if (gnode->memlist == user && ((gnode->memlist)->status == ST_GROUP || (gnode->memlist)->status == ST_EXIT)) { //check if found in the head
    gnode->memlist = (gnode->memlist)->link;
  } else { //not in the head
    NODE *p, *q;
    p = gnode->memlist;
    while (p) {
      q = p->link;
      if (q == user && (q->status == ST_GROUP || q->status == ST_EXIT)) {
        p->link = q->link;
      } else {
        p = p->link;
      }
    }
  }

  //put the user into the head of new group
  user->link = target_group->memlist;
  target_group->memlist = user;
  user->status = ST_CHAT;

  return target_group;
}
