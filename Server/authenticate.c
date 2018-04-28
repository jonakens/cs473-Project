#include "server.h"

extern sqlite3 *db;
extern GROUP *head;

int check_length (NODE *user, char input[], char error[])
{
  char msg[K];
  memset(msg, 0, K);

  if (strlen(input) < MIN || strlen(input) > MAX) {
    sprintf(msg, "[Invalid %s length] Enter 2-20 characters!\n", error);
    send_to_obuf(user, msg);
    return 0;
  }

  return 1;
}

int check_combination (NODE *user, char input[], char error[])
{
  char msg[K];
  memset(msg, 0, K);

  int i;
  for(i = 0; input[i] != '\0'; i++){
    if(!isalnum(input[i])) {
      sprintf(msg, "[Invalid %s combination] Alphanumeric only!\n", error);
      send_to_obuf(user, msg);
      return 0;
    }
  }

  return 1;
}

/*
  parsing argument for the commands. function will try to get 2 or 3
  argument depending on the user state
*/
void parse_args (status_st stat, char line[], char arg1[], char arg2[], char arg3[])
{
  int lp = 0, ap = 0;
  char c = line[lp];

  //getting rid of whitespaces before first token
  while (c == ' ' || c == '\t') c = line[++lp];

  //actually getting the first token
  while (c != '\0' && c != ' ' && c != '\t') {
    arg1[ap++] = c;
    c = line[++lp];
  }

  arg1[ap] = '\0';
  ap = 0;
  //check if there is anything else to read
  //if no second argument is empty string
  if (c == '\0') {
    arg2[ap] = '\0';
    return;
  }

  //there might be second argument
  //getting rid of whitespaces before second token
  while (c == ' ' || c == '\t') c = line[++lp];

  //decide what is the delim based on user status
  //bcs when status is private, message can have spaces
  char delim[5];
  if (stat == ST_PRIVATE) {
    strcpy(delim, "\0");
  } else {
    strcpy(delim, " \t");
  }

  //actually getting the second token
  while (strchr(delim, c) == NULL) {
    arg2[ap++] = c;
    c = line[++lp];
  }

  arg2[ap] = '\0';

  //if the user status is passwd, try to get third argument
  if (stat == ST_PASSWD) {
    ap = 0;
    //if no argument given the third argument is empty string
    if (c == '\0') {
      arg3[ap] = '\0';
      return;
    }

    //getting rid of whitespaces before third token
    while (c == ' ' || c == '\t') c = line[++lp];

    //actually getting the third token
    while (strchr(delim, c) == NULL) {
      arg3[ap++] = c;
      c = line[++lp];
    }

    arg3[ap] = '\0';
  }
}

/*
  initiate checking username and password with the database
*/
void check_credentials (status_st status, NODE *user, char credentials[])
{
  char username[K], password[K];
  parse_args(status, credentials, username, password, NULL);

  if (check_length(user, username, "username") == 0) {
    user->status = ST_MENU;
    return;
  }

  if (check_combination(user, username, "username") == 0) {
    user->status = ST_MENU;
    return;
  }

  char msg[K];
  memset(msg, 0, K);

  switch (status) {
    case ST_LOGIN:
      if (find_user(username)) {
        user->name = strdup(username);
        check_password(status, user, password);
      } else {
        user->status = ST_MENU;
        sprintf(msg, "[Username not found]\n");
        send_to_obuf(user, msg);
      }
      return;
    case ST_REGISTER:
      if (find_user(username)) {
        user->status = ST_MENU;
        sprintf(msg, "[Username already exists]\n");
        send_to_obuf(user, msg);
      } else {
        user->name = strdup(username);
        check_password(status, user, password);
      }
      return;
  }
}

/*
  find out if user is in the database
*/
int find_user (char username[])
{
  char sql[K];
  sprintf(sql, "SELECT * FROM account WHERE username='%s';", username);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Problem finding the user in the database] %s\n", sqlite3_errmsg(db));
    return 0;
  }

  int step =  sqlite3_step(res);
  sqlite3_finalize(res);
  if (step == SQLITE_ROW) {
    return 1;
  } else {
    return 0;
  }
}

/*
  check the password against the database
*/
void check_password (status_st status, NODE *user, char password[])
{
  if (check_length(user, password, "password") == 0) {
    user->status = ST_MENU;
    return;
  }

  if (check_combination(user, password, "password") == 0) {
    user->status = ST_MENU;
    return;
  }

  char msg[LONGSTR];
  char *newpasshash = NULL;
  memset(msg, 0, LONGSTR);

  switch (status) {
    case ST_LOGIN:
      if ((newpasshash = validate_password(user->name, password)) != NULL) {
        user->status = ST_CHAT;
        user->hash = strdup(newpasshash);
        user->login_time = time(NULL);
        sprintf(msg, "Welcome, %s\n", user->name);
        send_to_obuf(user, msg);
      } else {
        user->status = ST_MENU;
        sprintf(msg, "[Password does not match]\n");
        send_to_obuf(user, msg);
      }
      return;
    case ST_REGISTER:
      if ((newpasshash = make_password(user->name, password)) != NULL){
        user->status = ST_CHAT;
        user->hash = strdup(newpasshash);
        user->login_time = time(NULL);
        sprintf(msg, "[Account created successfully]\nWelcome, %s\n", user->name);
        send_to_obuf(user, msg);
      } else {
        user->status = ST_MENU;
        sprintf(msg, "[Problem creating new account]\n");
        send_to_obuf(user, msg);
      }
      return;
  }
}

/*
  actual password checking
*/
char *validate_password (char *username, char key[])
{
  char sql[K];
  sprintf(sql, "SELECT * FROM account WHERE username='%s';", username);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Problem validating with password database] %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  int step;
  while ((step = sqlite3_step(res)) == SQLITE_ROW) {
    char *tok = (char *) sqlite3_column_text(res, 2);
    char *hash = strdup(tok);
    char *p[2];
    p[0] = strtok(tok+1, "$");
    if (!p[0]) return NULL;
    p[1] = strtok(0, "$");
    if (!p[1]) return NULL;

    char salt[K];
    sprintf(salt, "$%s$%s$", p[0], p[1]);

    char *cipher = NULL;
    cipher = (char *) crypt(key, salt);

    if (strcmp(hash, cipher) == 0) return hash;
  }

  sqlite3_finalize(res);
  return NULL;
}

/*
  make new password hash and insert to the database
*/
char *make_password(char *username, char key[])
{
  char sugar[K];
  int i;
  srand(time(0));
  for (i = 0; i < 8; i++) {
    sugar[i] = 'a' + (rand() % 26);
  }
  sugar[K-1] = '\0';

  char salt[K];
  sprintf(salt, "$6$%s$", sugar);
  char *cipher = NULL;
  cipher = (char *) crypt(key, salt);
  char sql[K];
  sprintf(sql, "INSERT INTO account(username, hash) VALUES('%s', '%s');", username, cipher);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Problem inserting new account into database] %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  if (sqlite3_step(res) == SQLITE_DONE) {
    sqlite3_finalize(res);
    return cipher;
  } else {
    return NULL;
  }
}

/*
  changing the user password
*/
void change_password (NODE *user, char credentials[])
{
  char username[K], oldpass[K], newpass[K];
  parse_args(user->status, credentials, username, oldpass, newpass);

  if (check_length(user, username, "username") == 0) {
    user->status = ST_CHAT;
    return;
  }

  if (check_combination(user, username, "username") == 0) {
    user->status = ST_CHAT;
    return;
  }

  if (check_length(user, oldpass, "current password") == 0) {
    user->status = ST_CHAT;
    return;
  }

  if (check_combination(user, oldpass, "current password") == 0) {
    user->status = ST_CHAT;
    return;
  }

  if (check_length(user, newpass, "new password") == 0) {
    user->status = ST_CHAT;
    return;
  }

  if (check_combination(user, newpass, "new password") == 0) {
    user->status = ST_CHAT;
    return;
  }

  char msg[K];
  memset(msg, 0, K);

  if (find_user(username)) {
    if (validate_password(username, oldpass) != NULL) {
      if (update_password(username, newpass) == 1) {
        user->status = ST_CHAT;
        sprintf(msg, "[Password updated successfully]\n");
        send_to_obuf(user, msg);
      } else {
        user->status = ST_CHAT;
        sprintf(msg, "[Failed to change password]\n");
        send_to_obuf(user, msg);
      }
    } else {
      user->status = ST_CHAT;
      sprintf(msg, "[Current password does not match]\n");
      send_to_obuf(user, msg);
    }
  } else {
    user->status = ST_CHAT;
    sprintf(msg, "[Username not found]\n");
    send_to_obuf(user, msg);
  }
}

/*
  make password change permanent by updating the database
*/
int update_password (char *username, char key[])
{
  char sugar[K];
  int i;
  srand(time(0));
  for (i = 0; i < 8; i++) {
    sugar[i] = 'a' + (rand() % 26);
  }
  sugar[K-1] = '\0';

  char salt[K];
  sprintf(salt, "$6$%s$", sugar);
  char *cipher = NULL;
  cipher = (char *) crypt(key, salt);
  char sql[K];
  sprintf(sql, "UPDATE account SET hash='%s' WHERE username='%s';", cipher, username);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Problem updating password database] %s\n", sqlite3_errmsg(db));
    return 0;
  }

  if (sqlite3_step(res) == SQLITE_DONE) {
    sqlite3_finalize(res);
    return 1;
  } else {
    return 0;
  }
}
