#include "server.h"

extern GROUP *head;

/*
  handle all the execution of commands
*/
void command_handler(GROUP *group, NODE *user, token_t cmd, char *args)
{
  char msg[LONGSTR];
  memset(msg, 0, LONGSTR);

  switch (cmd) {
    case T_STOP:
      user->status = ST_BAD; //change the status, so that it can be reaped by the remove_stuff function later
      return;
    case T_HELP:
      print_help(user); //call the function to print help message
      print_prompt(group, user);
      return;
    case T_WHO:
      //get list of online users in the whole server
      //command only work for online (logged in) users
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
        return;
      }
      who_is_online(NULL, user);
      print_prompt(group, user);
      return;
    case T_LOOK:
      //get list of online users in the group
      //command only work for online (logged in) users
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
        return;
      }
      who_is_online(group, user);
      print_prompt(group, user);
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
      print_prompt(group, user);
      return;
    case T_LOGIN:
      //get the user into the general chat room
      //cannot login when already logged in
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_LOGIN
      if (strlen(args) > 0) {
        if (user->status == ST_CHAT) {
          sprintf(msg, "[Already logged in, please first logout]\n");
          send_to_obuf(user, msg);
          print_prompt(group, user);
        } else {
          user->status = ST_LOGIN;
          send_to_ibuf(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      }
      return;
    case T_REGISTER:
      //creating new user Account
      //cannot make one when already login
      //has to be in the menu ST_MENU state to make one
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_REGISTER
      if (strlen(args) > 0) {
        if (user->status == ST_CHAT) {
          sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
          send_to_obuf(user, msg);
          print_prompt(group, user);
        } else {
          user->status = ST_REGISTER;
          send_to_ibuf(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      }
      return;
    case T_PASSWD:
      //change the user password
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_PASSWD
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
          print_prompt(group, user);
          return;
        }

        user->status = ST_PASSWD;
        send_to_ibuf(user, args);
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      }
      return;
    case T_PRIVATE:
      //send a private message to a users
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_PRIVATE
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
          print_prompt(group, user);
          return;
        }

        user->status = ST_PRIVATE;
        send_to_ibuf(user, args);
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      }
      return;
    case T_GROUP:
      //make a group if not already exist
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //for later processing stage after the state changed to ST_GROUP
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
          print_prompt(group, user);
          return;
        }

        user->status = ST_GROUP;
        send_to_ibuf(user, args);
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      }
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
        print_prompt(group, user);
        return;
      }

      user->status = ST_EXIT;
      send_to_ibuf(user, "general");
      return;
  }
}

/*
  function to print help information
*/
void print_help(NODE *user)
{
  char help_message[K] = "[Command References]\n\
  Login to chat room         /login <uname> <pwd>\n\
  Logout from chat room      /logout\n\
  Register new account       /register <uname> <pwd>\n\
  Change password            /passwd <uname> <current pwd> <new pwd>\n\
  Send private message       /private <uname> <msg>\n\
  Make new group             /group <gname>\n\
  Exit group                 /exit\n\
  List all online users      /who\n\
  List online group members  /look\n\
  Print help message         /help\n\
  Disconnect from server     /stop\n";

  send_to_obuf(user, help_message);
}

/*
  finding out who are online
*/
void who_is_online (GROUP *group, NODE *user)
{
  char list[LONGSTR];
  char person[K];
  memset(list, 0, LONGSTR);

  GROUP *g;
  NODE *p;
  if (group != NULL) { //online for specific group
    for (p = group->memlist; p != NULL; p = p->link) {
      if (p->status == ST_CHAT) {
        memset(person, 0, K);
        sprintf(person, "[%s: %s]\n", p->name, p->addr);
        strcat(list,person);
      }
    }
  } else { //online for the whole chat server
    for (g = head; g != NULL; g = g->link) {
      for (p = g->memlist; p != NULL; p = p->link) {
        if (p->status == ST_CHAT) {
          memset(person, 0, K);
          sprintf(person, "[%s: %s]\n", p->name, p->addr);
          strcat(list,person);
        }
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
  
	int msg_send = 0;
  
	GROUP *g;
  NODE *p;
  
	for (g = head; g != NULL; g = g->link) {
    for (p = g->memlist; p != NULL; p = p->link) {
      if (p->status != ST_CHAT) continue;
      if (strcmp(p->name, username) == 0) {
			  msg_send = 1;
				if (p == user) { //if send message to the sender
          sprintf(line, "[Private] [%s: %s] %s\n", user->name, user->addr, message);
				  send_to_obuf(p, line);
				} else { //if send message to other user
          sprintf(line, "\n[Private] [%s: %s] %s\n", user->name, user->addr, message);
				  send_to_obuf(p, line);
          print_prompt(g, p);
				}  
      }
    }
  }

	memset(line, 0, LONGSTR);
	if (msg_send == 1) {
    sprintf(line, "[Message sent]\n");
	} else {
    sprintf(line, "[Receiver not found]\n");
	}
	send_to_obuf(user, line);
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
