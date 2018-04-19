#include "server.h"

#define MAX     20
#define MIN      2

extern NODE *head;

void parse_args (char credentials[], char name[], char pass[]);
int find_user (char username[]);
void check_password (status_st status, NODE *user, char password[]);
char *validate_password (char *username, char key[]);
char *make_password(char *username, char key[]);

extern sqlite3 *db;

void check_credentials (status_st status, NODE *user, char credentials[])
{
  char username[K], password[K];
  parse_args(credentials, username, password);

  char msg[K];
  memset(msg, 0, K);

  if (strlen(username) < MIN || strlen(username) > MAX) {
    sprintf(msg, "[Invalid Username Length] Enter 2-20 Characters!\n");
    send_to_obuf(user, msg);
    user->status = ST_MENU;
    return;
  }

  int i;
  for(i = 0; username[i] != '\0'; i++){
    if(!isalnum(username[i]) || isupper(username[i])) {
      sprintf(msg, "[Invalid Username Combination] Lowercase Alphanumeric Only!\n");
      send_to_obuf(user, msg);
      user->status = ST_MENU;
      return;
    }
  }

  switch (status) {
    case ST_LOGIN:
      if (find_user(username)) {
        user->name = strdup(username);
        check_password(status, user, password);
      } else {
        user->status = ST_MENU;
        sprintf(msg, "[Username Not Found]\n");
        send_to_obuf(user, msg);
      }
      return;
    case ST_REGISTER:
      if (find_user(username)) {
        user->status = ST_MENU;
        sprintf(msg, "[Username Already Exists]\n");
        send_to_obuf(user, msg);
      } else {
        user->name = strdup(username);
        check_password(status, user, password);
      }
      return;
  }
}

void private_message (NODE *user, char argument[])
{
  user->status = ST_CHAT;
  char username[K], message[K], line[LONGSTR];
  parse_args(argument, username, message);
  int flag = 0;

  NODE *p;
  for (p = head; p != NULL; p = p->link) {
    if (p->status != ST_CHAT) continue;
    if (strcmp(p->name, username) == 0) {
      flag = 1; //report user found
      sprintf(line, "[Private Message From %s] %s\n", user->name, message);
      send_to_obuf(p, line);
    }
  }

  memset(line, 0, LONGSTR);
  if (flag == 0) {
    sprintf(line, "[Receiver Offline]\n");
    send_to_obuf(user, line);
  } else {
    sprintf(line, "[Message Sent]\n");
    send_to_obuf(user, line);
  }
}

void parse_args (char credentials[], char name[], char pass[])
{
  int lp = 0, ap = 0;
  char c = credentials[lp];

  while (c == ' ' || c == '\t') c = credentials[++lp];

  while (c != '\0' && c != ' ' && c != '\t') {
    name[ap++] = c;
    c = credentials[++lp];
  }

  name[ap] = '\0';
  ap = 0;
  if (c == '\0') {
    pass[ap] = '\0';
    return;
  }

  while (c == ' ' || c == '\t') c = credentials[++lp];

  while (c != '\0') {
    pass[ap++] = c;
    c = credentials[++lp];
  }

  pass[ap] = '\0';
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
    sprintf(msg, "[Invalid Password Length] Enter 2-20 Characters!\n");
    send_to_obuf(user, msg);
    user->status = ST_MENU;
    return;
  }

  int i;
  for(i = 0; password[i] != '\0'; i++){
    if(!isalnum(password[i])) {
      sprintf(msg, "[Invalid Password Combination] Alphanumeric Only!\n");
      send_to_obuf(user, msg);
      user->status = ST_MENU;
      return;
    }
  }

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
