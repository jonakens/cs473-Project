#include "server.h"

#define MAX     20
#define MIN      2

int find_user (char username[]);
char *validate_password (char *username, char key[]);
char *make_password(char *username, char key[]);

extern sqlite3 *db;

void check_username (status_st status, NODE *user, char username[])
{
  char msg[LONGSTR];
  memset(msg, 0, LONGSTR);

  if (strlen(username) < MIN || strlen(username) > MAX) {
    if (status == ST_LOGIN) {
      sprintf(msg, "[Invalid Length] Enter 2-20 Characters!\nUsername: ");
    } else {
      sprintf(msg, "[Invalid Length] Enter 2-20 Characters!\nNew Username: ");
    }
    send_to_obuf(user, msg);
    return;
  }

  int i;
  for(i = 0; username[i] != '\0'; i++){
    if(!isalnum(username[i]) || isupper(username[i])) {
      if (status == ST_LOGIN) {
        sprintf(msg, "[Invalid Combination] Lowercase Alphanumeric Only!\nUsername: ");
      } else {
        sprintf(msg, "[Invalid Combination] Lowercase Alphanumeric Only!\nNew Username: ");
      }
      send_to_obuf(user, msg);
      return;
    }
  }

  if (find_user(username)) {
    if (status == ST_LOGIN) {
      user->name = strdup(username);
      sprintf(msg, "Password: ");
      send_to_obuf(user, msg);
      user->status = ST_PASSWD;
    } else if (status == ST_NEWUSR) {
      sprintf(msg, "[User Already Exists]\nNew Username: ");
      send_to_obuf(user, msg);
    }
  } else {
    if (status == ST_LOGIN) {
      sprintf(msg, "[User Not Found]\nUsername: ");
      send_to_obuf(user, msg);
    } else if (status == ST_NEWUSR) {
      user->name = strdup(username);
      sprintf(msg, "New Password: ");
      send_to_obuf(user, msg);
      user->status = ST_NEWPWD;
    }
  }
  return;
}

int find_user (char username[])
{
  char sql[K];
  sprintf(sql, "select * from account where username='%s';", username);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Database Find User]: %s\n", sqlite3_errmsg(db));
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

void check_password (status_st status, NODE *user, char password[])
{
  char msg[LONGSTR];
  char *newpasshash = NULL;

  memset(msg, 0, LONGSTR);

  if (strlen(password) < MIN || strlen(password) > MAX) {
    if (status == ST_PASSWD) {
      sprintf(msg, "[Invalid Length] Enter 2-20 Characters!\nPassword: ");
    } else {
      sprintf(msg, "[Invalid Length] Enter 2-20 Characters!\nNew Password: ");
    }
    send_to_obuf(user, msg);
    return;
  }

  int i;
  for(i = 0; password[i] != '\0'; i++){
    if(!isalnum(password[i])) {
      if (status == ST_PASSWD) {
        sprintf(msg, "[Invalid Combination] Alphanumeric Only!\nPassword: ");
      } else {
        sprintf(msg, "[Invalid Combination] Alphanumeric Only!\nNew Password: ");
      }
      send_to_obuf(user, msg);
      return;
    }
  }

  switch (status) {
    case ST_PASSWD:
      if ((newpasshash = validate_password(user->name, password)) != NULL) {
        user->hash = strdup(newpasshash);
        user->status = ST_CHAT;
        sprintf(msg, "Welcome, %s\n", user->name);
        send_to_obuf(user, msg);
      } else {
        sprintf(msg, "[Password does not match]\nPassword: ");
        send_to_obuf(user, msg);
      }
      return;
    case ST_NEWPWD:
      if ((newpasshash = make_password(user->name, password)) != NULL){
        user->status = ST_CHAT;
        user->hash = strdup(newpasshash);
        sprintf(msg, "[Account created successfully]\nWelcome, %s\n", user->name);
        send_to_obuf(user, msg);
      } else {
        user->status = ST_NEWUSR;
        sprintf(msg, "[Problem creating new account]\nNew Username: ");
        send_to_obuf(user, msg);
      }
      return;
  }
}

char *validate_password (char *username, char key[])
{
  char sql[K];
  sprintf(sql, "select * from account where username='%s';", username);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Database Validate Password]: %s\n", sqlite3_errmsg(db));
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
  sprintf(sql, "insert into account(username, hash) values('%s', '%s');", username, cipher);
  sqlite3_stmt *res;

  int ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
  if (ret != SQLITE_OK) {
    fprintf(stderr, "[Database Make Password] %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  if (sqlite3_step(res) == SQLITE_DONE) {
    sqlite3_finalize(res);
    return cipher;
  } else {
    return NULL;
  }
}
