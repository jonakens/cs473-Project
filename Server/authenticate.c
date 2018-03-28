#include "server.h"
int checkname(char s[])   // all lower case letters, at most MAXNAMELEN characters
{
    int i;
    
    for(i=0;s[i];i++){
        if(i > MAXNAME)
            return 0;
        if(s[i] == '\n'){
            s[i] = 0;
            return 1;
        }
        if(!islower(s[i]))
            return 0;
    }
    return 0;
}
int checkpasswd(char *user, char *plaintext)
{
  char ciphertext[K];

  if(!finduser(user, ciphertext))
    return 0;
  return checkpass(user,plaintext,ciphertext);
}
char *finduser(char *key, char epass[])
{
  FILE *fd;
  char b[K];
  char *p,*q;
  int keylen;

  keylen = strlen(key);
  fd = fopen(PASSWD,"r");
  while(fgets(b,K,fd)){
    p = strtok(b,":\n");    // returns first string ending in color or newline
    if(strcmp(p,key)==0){   // that is a valid user name:
      q = strtok(0,":\n");
      strcpy(epass,q);
      fclose(fd);
      return epass;
    }
  }
  fclose(fd);
  return 0;
}
int checkpass(char *key, char *plaintext, char *ciphertext)
{
  char *p[2], salt[K];
  char a[K],b[K];
  int i;

  strcpy(a,ciphertext);
  p[0] = strtok(ciphertext+1,"$");
  if(!p[0]) return 0;
  p[1] = strtok(0,"$");
  if(!p[1]) return 0;
  sprintf(salt,"$%s$%s$",p[0],p[1]);
  sprintf(b,"%s",crypt(plaintext,salt));
#ifdef DEBUG
  fprintf(stderr,"%s\n",a);
  fprintf(stderr,"%s\n",b);
#endif
  return (strcmp(a,b) == 0);
}
#ifdef ONLINEPASSWDS
char *makepass(char *key, char *plaintext)
{
  FILE *fd;
  char sugar[SLEN];
  char salt[K];
  char *cipher;
  int i;

  for(i=0;i<SLEN-1;i++)
    sugar[i] = 'a' + random() % 26;
  sugar[SLEN-1] = 0;                         // sugar is a random string of letters
  sprintf(salt,"$5$%s$", sugar);
  cipher = (char *) crypt(plaintext, salt);  // don't forget -lcrypt when compiling
  fd = fopen(PASSWD,"a");                    // open for appending - read+write+start at EOF
  if(!fd){
    fprintf(stderr,"Error in makepass?\n");
    exit(0);
  }
  fprintf(fd,"%s:%s\n",key,cipher);
  fclose(fd);
}
#endif

int isUserRegistered(char *key){ //checks if user is actually in the password file
  
  FILE *fd;
  char b[K];
  char *p,*q;
  int keylen;

  keylen = strlen(key);
  //fprintf(stderr, "namelen = %d\n", keylen);
  fd = fopen(PASSWD,"r");
 
  while(fgets(b,K,fd)){
    p = strtok(b,":");    // returns first string ending in color or newline

    if(strcmp(p,key)==0){ // that is a valid user name:
      fprintf(stderr, "User %s found\n", key);
      fclose(fd);
      return 1;
    }
  }
  fclose(fd);
  fprintf(stderr, "User %s not found\n", key);
  return 0;
  
}
