#include "server.h"

extern NODE *head;

void who_is_online (NODE *user);

void command_handler(NODE *user, token_t cmd)
{
  char msg[K];
  memset(msg, 0, K);

  switch (cmd) {
    case T_QUIT:
      user->status = ST_BAD;
      return;
    case T_HELP:
      print_help(user);
      return;
    case T_LOGIN:
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Already logged in, please first logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_LOGIN;
        sprintf(msg, "Username: ");
        send_to_obuf(user, msg);
      }
      return;
    case T_LOGOUT:
      if (user->status == ST_CHAT) {
        user->status = ST_MENU;
        print_help(user);
      } else {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      }
      return;
    case T_NEW:
      if (user->status == ST_CHAT) {
        sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_NEWUSR;
        sprintf(msg, "New Username: ");
        send_to_obuf(user, msg);
      }
      return;
    case T_WHO:
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        return;
      }
      who_is_online(user);
      return;
  }
}

void who_is_online (NODE *user)
{
  NODE *p;
  char list[K] = "\n";
  char person[K];
  int counter = 0;

  for (p = head; p != NULL; p = p->link) {
    if (p->status == ST_CHAT) {
      memset(person, 0, K);
      sprintf(person, "[%d] %s (%s)\n", counter, p->name, p->addr);
      strcat(list,person);
      counter++;
    }
  }

  send_to_obuf(user, list);
}

void print_help(NODE *user)
{
  char help_message[K] = "Welcome to the home menu of the chat server!\nYou can use the following commands:\n\
  /help    Print this message\n\
  /quit    Close the connection to the chat server\n\
  /login   Enter the general chat room\n\
  /logout  Exit from all chat room\n\
  /who     List online user in the room\n\
  /new     Make a new account and login\n";

  send_to_obuf(user, help_message);
}
