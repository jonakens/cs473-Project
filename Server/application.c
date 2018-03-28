#include "server.h"

extern NODE *head;

void sendToChat(NODE *p, char *s){
 
  NODE *q;
  
  for(q = head ; q != NULL ; q = q->link){
      if(q->status != ST_CHAT || q == p)
        continue;
      sendToUser(q, s);
  }
    
}
void sendToUser(NODE *user, char *s) {
  
  MESS *p;
  
  p = createMessage(s);
  
  if(user->head == NULL){
    user->head = user->tail = p;
  } else {
    user->tail->messlink = p;
    user->tail = p;
  }
  
}

MESS *createMessage(char mess[]) {
  
  MESS *p;
  
  p = malloc(sizeof(MESS));
  p->message = strdup(mess);
  p->messlink = 0;
  return p;
}


void process_stuff(int sock){
  
  NODE *p;
  int len;
  
  for(p = head ; p != NULL ; p = p->link){
     if(p->status == ST_BAD)
       continue;
     if(p->status == ST_LOGIN && p->uflag != 1){
       sendToUser(p, "Username: ");
       p->uflag = 1;
     }
     
     
      if(*p->ibuf){        // has user p sent a message      
        switch(p->status){
          case ST_BAD: 
             break;     
          case ST_LOGIN:
	          if(isUserRegistered(p->ibuf)){
	            p->status = ST_PASSWD;
	            *p->ibuf = 0;
	            break;
	          } else {
	            //sendToUser(p, "Account not found, contact administrator for an account\n");
	            //eventally put in a registration feature here
	          }  
            if(checkname(p->ibuf)){ //if the name meets the required parameters?
                p->uname = strdup(p->ibuf);
                p->status = ST_PASSWD;
		            sendToUser(p, "Password: ");
                *p->ibuf = 0;
                break;
            } else { 
                sendToUser(p, "Invalid Name\n");
                *p->ibuf = 0;
	              p->status = ST_LOGIN;
                break;
            }  
          case ST_PASSWD:
	          len = strlen(p->ibuf);
              p->ibuf[len-1] = 0; //gets rid of newline in buffer, otherwise it will choke
              if(checkpasswd(p->uname, p->ibuf)){
	              sendToUser(p,"Success!\n");
                p->loginTime = time(0);
                p->status = ST_CHAT;
                *p->ibuf = 0;
                break;
              } else {
	              sendToUser(p,"Wrong Password\n");
                p->status = ST_LOGIN;
		            p->uflag = 0;
                *p->ibuf = 0;
                break;
              }   
          case ST_CHAT:
            //may want to check length of ibuf here, do something if limit is exceeded 
            len = strlen(p->ibuf);
           
            if(len > CHARLIMIT){ //character limit was exceeded
              *p->ibuf = 0;
              break; //just don't send the message
            }
            
            if(strcmp(p->ibuf, "\n") == 0){//user only sent a newline character
              *p->ibuf = 0;
              continue;
            }

            //look at beginning of line to see if it is a command
            if(p->ibuf[0] == COMMAND){
              //if it is a command, then handle it.
              commandHandler(p->ibuf,p);
              //you dont want to send it to the chat so break
              *p->ibuf = 0;
              break;
            }

            sprintf(p->obuf, "%s: %s", p->uname, p->ibuf);       
	          sendToChat(p, p->obuf);
	          *p->ibuf = 0;
	          *p->obuf = 0;
            
	          break;
	        default: //we shouldn't ever see this, at least not yet
            break;
	    }  
    }
  }
}

void write_stuff(int sock) {
   NODE *p;
   fd_set oset, oset2;
   int nbytes;             // number of bytes read
   int dmax = 0;
   struct timeval zero;

   zero.tv_sec = 0;
   zero.tv_usec = 0;
   FD_ZERO(&oset);
   for(p = head ; p != NULL ; p = p->link){
      if(p->status == ST_BAD)
         continue;
      if(p->head){
         FD_SET(p->desc, &oset);
         if(p->desc > dmax)
            dmax = p->desc;
      }
   }
   select(dmax+1, 0, &oset, 0, &zero);
   for(p = head ; p != NULL ; p = p->link)
      if(FD_ISSET(p->desc, &oset))
         sendMessage(p);
      
}

void printoutput(MESS *p) {
  while(p != NULL){
    printf("%s\n", p->message);
    p = p->messlink;
  }
}

MESS *removehead(MESS *h) {
  MESS *newhead;

  if(!h)
    return 0;
  newhead = h->messlink;
  free(h->message);            // free memory used by the string
  free(h);               // free memory used by list node
  return newhead;
}

void sendMessage(NODE *user) {
    
   MESS *p, *q;
   int rv, len;

   if(!user->head)    // should not occur, already checked in write_stuff
      return;
   p = user->head;
   len = strlen(p->message);
   rv = write(user->desc, p->message, len);

   if(rv < len){
     user->status = ST_BAD;
     return;
   }
   user->head = p->messlink;
   free(p->message);
   free(p);
}
