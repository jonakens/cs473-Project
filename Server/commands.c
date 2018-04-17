#include "server.h"

extern NODE *head;

void who_is_online (NODE *user);

void command_handler(NODE *user, token_t cmd, char *args)
{
  char msg[LONGSTR];
  memset(msg, 0, LONGSTR);

  switch (cmd) {
    case T_QUIT:
      user->status = ST_BAD;
      return;
    case T_HELP:
      print_help(user);
      return;
    case T_WHO:
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }
      who_is_online(user);
      return;
    case T_LOGOUT:
      if (user->status == ST_CHAT) {
        user->status = ST_MENU;
      } else {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      }
      return;
    case T_LOGIN:
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Already logged in, please first logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_LOGIN;
        send_to_ibuf(user, args);
      }
      return;
    case T_REGISTER:
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_REGISTER;
        send_to_ibuf(user, args);
      }
      return;
    case T_PRIVATE:
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }

      user->status = ST_PRIVATE;
      send_to_ibuf(user, args);
      return;
  }
}

void who_is_online (NODE *user)
{
  NODE *p;
  char list[K] = "\n";
  char person[K];

  for (p = head; p != NULL; p = p->link) {
    if (p->status == ST_CHAT) {
      memset(person, 0, K);
      sprintf(person, "[%s:%s]\n", p->name, p->addr);
      strcat(list,person);
    }
  }

  send_to_obuf(user, list);
}

void print_help(NODE *user)
{
  char help_message[K] = "You can use the following commands:\n\
  /login     <username> <password>  Enter the chat room\n\
  /logout                           Exit the chat room\n\
  /register  <username> <password>  Make a new account and login\n\
  /private   <username> <message>   Send a private message to <username>\n\
  /who                              List online user in the room\n\
  /help                             Print this message\n\
  /quit                             Close the connection to the chat server\n";

  send_to_obuf(user, help_message);
}
