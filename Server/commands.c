#include "server.h"

extern sqlite3 *db;
extern GROUP *head;

/*
  handle all the execution of commands
*/
void command_handler(GROUP *group, NODE *user, token_t tok, char *args)
{
  char msg[LONGSTR];
  memset(msg, 0, LONGSTR);

  switch (tok) {
    case T_STOP:
      if (user->status == ST_ANSWER) {
        user->status = ST_CHAT;
        print_prompt(group, user);
      } else {
        user->status = ST_BAD; //change the status, so that it can be reaped by the remove_stuff function later
      }
      return;
    case T_HELP:
      if (user->status == ST_ANSWER) return;
      //call the function to print help message
      print_help(user);
      print_prompt(group, user);
      return;
    case T_WHO:
      if (user->status == ST_ANSWER) return;
      //get list of online users in the whole server
      //command only work for online (logged in) users
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      } else {
        who_is_online(NULL, user);
      }
      print_prompt(group, user);
      return;
    case T_LOOK:
      if (user->status == ST_ANSWER) return;
      //get list of online users in the group
      //command only work for online (logged in) users
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      } else {
        who_is_online(group, user);
      }
      print_prompt(group, user);
      return;
    case T_LOGOUT:
      if (user->status == ST_ANSWER) return;
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
      if (user->status == ST_ANSWER) return;
      //get the user into the general chat room
      //cannot login when already logged in
      //the command should have arguments which is kept in args
      if (strlen(args) > 0) {
        if (user->status == ST_CHAT) {
          sprintf(msg, "[Already logged in, please first logout]\n");
          send_to_obuf(user, msg);
        } else if (user->status == ST_MENU) {
          user->status = ST_LOGIN;
          check_credentials(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
      }
      print_prompt(group, user);
      return;
    case T_REGISTER:
      if (user->status == ST_ANSWER) return;
      //creating new user Account
      //cannot make one when already login
      //has to be in the menu ST_MENU state to make one
      //the command should have arguments which is kept in args
      if (strlen(args) > 0) {
        if (user->status == ST_CHAT) {
          sprintf(msg, "[Unable to create new account while logging in, please logout]\n");
          send_to_obuf(user, msg);
        } else if (user->status == ST_MENU) {
          user->status = ST_REGISTER;
          check_credentials(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
      }
      print_prompt(group, user);
      return;
    case T_PASSWD:
      if (user->status == ST_ANSWER) return;
      //change the user password
      //command only work for online (logged in) users
      //the command should have arguments which is kept in args
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
        } else {
          user->status = ST_PASSWD;
          change_password(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
      }
      print_prompt(group, user);
      return;
    case T_PRIVATE:
      if (user->status == ST_ANSWER) return;
      //send a private message to a users
      //command only work for online (logged in) users
      //the command should have arguments which is kept in args
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
        } else {
          user->status = ST_PRIVATE;
          private_message(user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
      }
      print_prompt(group, user);
      return;
    case T_GROUP:
      if (user->status == ST_ANSWER) return;
      //make a group if not already exist
      //command only work for online (logged in) users
      //the command should have arguments which is kept in args
      if (strlen(args) > 0) {
        if (user->status != ST_CHAT) {
          sprintf(msg, "[You are not logged in]\n");
          send_to_obuf(user, msg);
        } else {
          user->status = ST_GROUP;
          group = manage_group(group, user, args);
        }
      } else {
        sprintf(msg, "[No argument]\n");
        send_to_obuf(user, msg);
      }
      print_prompt(group, user);
      return;
    case T_EXIT:
      if (user->status == ST_ANSWER) return;
      //exit from a group and go to general room
      //command only work for online (logged in) users
      //the command has arguments which is kept in the user node
      //it is an alias for /group general
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_EXIT;
        group = manage_group(group, user, args);
      }
      print_prompt(group, user);
      return;
    case T_QUIZ:
      if (user->status != ST_CHAT) {
        sprintf(msg, "[You are not logged in]\n");
        send_to_obuf(user, msg);
        print_prompt(group, user);
      } else {
        sprintf(msg, "[You are in trivia quiz mode, to exit the quiz type /stop]\n");
        send_to_obuf(user, msg);
        user->status = ST_QUESTION;
        get_question(group, user);
      }
      return;
  }
}

/*
  function to print help information
*/
void print_help(NODE *user)
{
  char help_message[K] = "        [Command References]\n\
  --- Description ---        -- cmd --   -- args --\n\
  Login to chat room         /login      <uname>  <pwd>\n\
  Logout from chat room      /logout\n\
  Register new account       /register   <uname>  <pwd>\n\
  Change password            /passwd     <uname>  <current pwd>  <new pwd>\n\
  Send private message       /private    <uname>  <msg>\n\
  Make/enter group           /group      <gname>  <gkey>\n\
  Exit group                 /exit\n\
  Enter trivia quiz mode     /quiz\n\
  List all online users      /who\n\
  List online group members  /look\n\
  Print help message         /help\n\
  Disconnect from server     /stop\n";

  send_to_obuf(user, help_message);
}

/*
  finding out who are online
*/
void who_is_online (GROUP *current_group, NODE *target_user)
{
  char list[LONGSTR];
  char person[K];
  memset(list, 0, LONGSTR);

  NODE *user;
  struct tm *login_time;
  char *formatted_time;
  int len;

  if (current_group != NULL) { //list of online member for specific group
    for (user = current_group->member; user != NULL; user = user->link) {
      if (user->status == ST_CHAT) {
        memset(person, 0, K);
        login_time = localtime(&(user->login_time));
        formatted_time = asctime(login_time);
        len = strlen(formatted_time);
        formatted_time[len-1] = '\0';
        sprintf(person, "[%s] %s (%s)\n", formatted_time, user->name, user->addr);
        strcat(list,person);
      }
    }
  } else { //list of online member for the whole chat server
    GROUP *group;
    for (group = head; group != NULL; group = group->link) {
      for (user = group->member; user != NULL; user = user->link) {
        if (user->status == ST_CHAT) {
          memset(person, 0, K);
          login_time = localtime(&(user->login_time));
          formatted_time = asctime(login_time);
          len = strlen(formatted_time);
          formatted_time[len-1] = '\0';
          sprintf(person, "[%s] %s (%s)\n", formatted_time, user->name, user->addr);
          strcat(list,person);
        }
      }
    }
  }

  send_to_obuf(target_user, list);
}

/*
  sending private message to particular user
  there could multiple user logged in with the
  same username
*/
void private_message (NODE *source_user, char argument[])
{
  char username[K], message[K], garbage[K], line[LONGSTR];
  parse_args(source_user->status, argument, username, message, garbage);
  source_user->status = ST_CHAT;

  int msg_send = 0;
  GROUP *group;
  NODE *user;

  for (group = head; group != NULL; group = group->link) {
    for (user = group->member; user != NULL; user = user->link) {
      if (user->status != ST_CHAT) continue;
      if (strcmp(user->name, username) == 0) {
        msg_send = 1;
        if (user == source_user) { //if send message to the sender, no need to print prompt here
          sprintf(line, "[Private message] %s (%s) says %s\n", user->name, user->addr, message);
          send_to_obuf(user, line);
        } else { //if send message to other user, print prompt to other users
          sprintf(line, "\n[Private message] %s (%s) says %s\n", user->name, user->addr, message);
          send_to_obuf(user, line);
          print_prompt(group, user);
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
  send_to_obuf(source_user, line);
}

/*
  move a user node from one group into another
*/
GROUP *manage_group (GROUP *current_group, NODE *user, char argument[])
{
  char group_name[K], group_key[K], garbage[K];
  parse_args(user->status, argument, group_name, group_key, garbage);
  user->status = ST_CHAT;

  if (check_length(user, group_name, "group name") == 0 || check_combination(user, group_name, "group name") == 0) {
    return current_group;
  }

  if (strcmp(group_name, "general") != 0) {
    if (check_length(user, group_key, "group key") == 0 || check_combination(user, group_key, "group key") == 0) {
      return current_group;
    }
  }

  //looking for the target group to move the user
  //by checking the name of the group, if found
  //put it in the target_group var
  GROUP *target_group = NULL, *group;
  for (group = head; group != NULL; group = group->link) {
    if (strcmp(group->name, group_name) == 0) {
      if (strcmp(group->name, "general") != 0) {
        if (strcmp(group->key, group_key) == 0) {
          target_group = group;
        } else {
          send_to_obuf(user, "[Group key does not match]\n");
          return current_group;
        }
      } else {
        target_group = group;
      }
      break;
    }
  }

  //check if target has been found, else make new one
  //not found when NULL, thus make new one and put it
  //at the end of the group list
  if (target_group == NULL) {
    target_group = make_group(group_name, group_key);
    for (group = head; group->link != NULL; group = group->link) {}
    group->link = target_group;
  }

  //take off the user node from the source group
  if (current_group->member == user) { //check if found in the head
    current_group->member = (current_group->member)->link;
  } else { //not in the head
    NODE *curr_user, *next_user;
    curr_user = current_group->member;
    while (curr_user) {
      next_user = curr_user->link;
      if (next_user == user) {
        curr_user->link = next_user->link;
      } else {
        curr_user = curr_user->link;
      }
    }
  }

  //put the user into the head of new group
  user->link = target_group->member;
  target_group->member = user;

  return target_group;
}

void get_question(GROUP *current_group, NODE *user)
{
  char sql[K], error[K];
  sqlite3_stmt *res;
  sprintf(sql, "SELECT count(*) FROM quiz;");
  memset(error, 0, K);

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    sprintf(error, "[Problem retrieving question]\n");
    send_to_obuf(user, error);
    user->status = ST_CHAT;
    print_prompt(current_group, user);
    return;
  }

  if (sqlite3_step(res) != SQLITE_ROW) {
    sprintf(error, "[Problem retrieving question]\n");
    send_to_obuf(user, error);
    user->status = ST_CHAT;
    print_prompt(current_group, user);
    return;
  }

  int total = sqlite3_column_int(res, 0);
  sqlite3_finalize(res);

  srand(time(0));
  int num = (rand() % total) + 1;

  memset(sql, 0, K);
  sprintf(sql, "SELECT * FROM quiz WHERE id=%d;", num);

  ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    sprintf(error, "[Problem retrieving question]\n");
    send_to_obuf(user, error);
    user->status = ST_CHAT;
    print_prompt(current_group, user);
    return;
  }

  if (sqlite3_step(res) != SQLITE_ROW) {
    sprintf(error, "[Problem retrieving question]\n");
    send_to_obuf(user, error);
    user->status = ST_CHAT;
    print_prompt(current_group, user);
    return;
  }

  char question[K], answer[K], formatedQuestion[K];
  memset(question, 0, K);
  memset(answer, 0, K);
  memset(formatedQuestion, 0, K);
  sprintf(question, "%s", (char *) sqlite3_column_text(res, 1));
  sprintf(answer, "%s", (char *) sqlite3_column_text(res, 2));
  sprintf(formatedQuestion, "[Question] %s (true/false): ", question);
  send_to_obuf(user, formatedQuestion);

  if (user->answer != NULL) {
    free(user->answer);
  }

  user->answer = strdup(answer);
  user->status = ST_ANSWER;
  sqlite3_finalize(res);

  return;
}

void validate_answer (GROUP *current_group, NODE *user, char answer[])
{
  int lp = 0, ap = 0;
  char ans[K];
  char c = answer[lp];

  while (isspace(c)) c = answer[++lp];
  while (!isspace(c) && c != '\0') {
    ans[ap++] = tolower(c);
    c = answer[++lp];
  }
  ans[ap] = '\0';

  char result[K];
  if (strncmp(ans, user->answer, 1) == 0) {
    sprintf(result, "[Correct]\n");
  } else {
    sprintf(result, "[Wrong] The answer is %s\n", user->answer);
  }
  send_to_obuf(user, result);

  user->status = ST_QUESTION;
  get_question(current_group, user);

  return;
}
