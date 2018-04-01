#include "server.h"

extern NODE *head;

void command_handler(NODE *user, token_t cmd)
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
    case T_LOGIN:
      if (user->status == ST_CHAT) {
        memset(msg, 0, LONGSTR);
        sprintf(msg, "[Already logged in, please first logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_LOGIN;
        memset(msg, 0, LONGSTR);
        sprintf(msg, "Username: ");
        send_to_obuf(user, msg);
      }
      return;
    case T_LOGOUT:
      if (user->status == ST_CHAT) {
        user->status = ST_MENU;
        print_help(user);
      } else {
        memset(msg, 0, LONGSTR);
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      }
      return;
    case T_NEW:
      if (user->status == ST_CHAT) {
        memset(msg, 0, LONGSTR);
        sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_NEWUSR;
        memset(msg, 0, LONGSTR);
        sprintf(msg, "New Username: ");
        send_to_obuf(user, msg);
      }
      return;
    case T_LIST:
      memset(msg, 0, LONGSTR);
      sprintf(msg, "[Not implemented yet]\n");
      send_to_obuf(user, msg);
      return;
  }
}

void print_help(NODE *user)
{
  char help_message[K] = "Welcome to the home menu of the chat server!\nYou can use the following commands:\n\
  /help    Print this message\n\
  /quit    Close the connection to the chat server\n\
  /login   Enter the general chat room\n\
  /logout  Exit from all chat room\n\
  /list    List online user in the room\n\
  /new     Make a new account and login\n";

  send_to_obuf(user, help_message);
}
