#include "server.h"

extern NODE *head;


void who(NODE *sender);                                     //prints who is online
void removeUser(NODE *sender);                              //removes the user from the server
void printHelp(NODE *sender);                               //prints help to the user who requested it
void writeTo(NODE *sender, char *recpt, char *message);     //writes <message> to <recpt> from <sender>
void toggleMessage(NODE *person);                           //toggles private messaging for the person.
void parseCommand(char *command);                           //parses command for further processing.
void getCommandType(char *command);
void getPersonNameFromCommand(char *command);

//cmd is the command being used
//user is the user that used the command
void commandHandler(char *cmd, NODE *user){
    int len = strlen(cmd);
    parseCommand(cmd);

    if(strcmp(cmdline[0], "/who\n") == 0)
    {
        who(user);
    }
    else if(strcmp(cmdline[0], "/quit\n") == 0)
    {
        removeUser(user);
    }
    else if(strcmp(cmdline[0], "/help\n")==0)
    {
        printHelp(user);
    }
    else if(strcmp(cmdline[0], "/write") == 0)
    {
        //getPersonNameFromCommand(cmd);
        writeTo(user, cmdline[1], cmdline[2]);
    }
    else if(strcmp(cmdline[0], "/mesg\n") == 0)
    {
        toggleMessage(user);
    }
    else
    {
        printHelp(user);
    }
}

void who(NODE *sender){
    NODE *p;
    struct tm* timeinfo;
    char messagebuffer[256];
    
    for(p = head; p != '\0'; p = p->link){
        timeinfo = localtime(&p->loginTime);
        sprintf(messagebuffer, "%12s %15s %20s \n", p->uname, p->addr, asctime(timeinfo));
        sendToUser(sender, messagebuffer);
    }
}

void removeUser(NODE *sender){
    char leaveMessage[128];
    sprintf(leaveMessage, "%s is leaving the server! Have a nice day! :~)\n", sender->uname);
    sendToChat(sender, leaveMessage);
    sender->status = ST_LOGIN;  //this user will be removed from the server.
    close(sender->desc);
}

void printHelp(NODE *sender){
    char *helpMessage[5] = {
        "/who - who - list all user names, IP\'s, and login times for all users online\n",
        "*/write - write <username> <message> - *****doesnt work yet.\n",
        "/mesg - mesg - toggles personal messages on or off\n",
        "/quit - quit - disconnects you from the server\n",
        "/help - help - displays this help menu\n"
    };

    for(int i = 0; i < 5; i++){
        sendToUser(sender, helpMessage[i]);
    }
}

/*WARNING DOES NOT WORK YET*/
void writeTo(NODE *sender, char recpt[], char message[]){  //sends message from sender to recpt
    fprintf(stderr, "[MESSAGE]%s --> %s: %s", sender->uname, recpt, message);
    
    NODE *p;
    char fullMessage[K];

    sprintf(fullMessage, "[%s] says: %s\n", sender->uname, message);
    for(p = head; p != '\0'; p = p->link){
        if(strcmp(recpt, p->uname)==0){  //if the user is found
            if(p->mesg == MESG_ON){                  //if the private message flag is enabled.
                fprintf(stderr, "[SYSTEM]User %s was found and PM is turned on. attempting to send a message now.\n", recpt);
                sendToUser(p,fullMessage);
                break;
            }else{                         //if the private message flag is disabled.
                sendToUser(sender, "Users messages are not turned on.\n");
                break;
            }
        }else{                           //if user isnt found
            sendToUser(sender, "User not found!\n");
            break;
        }
        *fullMessage = 0;                //free the memory after function is over
    }

}

void toggleMessage(NODE *person){
    NODE *p;

    for(p = head; p != NULL; p = p->link){
        if(p == person){
            if(p->mesg){
                sendToUser(person,"[Server] Private Messages was turned off!\n");
                p->mesg = MESG_OFF;
            }else{
                sendToUser(person,"[Server] Private Messages was turned on!\n");
                p->mesg = MESG_ON;
            }
        }
    }
}

void getCommandType(char *command){
    int i,j,k;
    char *cmd = NULL;
    j=k=0;
    for(i=0; command[i] != ' ' || command[i] != '\0'; i++){
        cmd[i] = command[i];
    }
    cmd[i] = '\0';

    if(*cmd != '\0'){
        printf("command: %s\n", cmd);
    }
    strcpy(cmdline[0],cmd);
}

void getPersonNameFromCommand(char *command){
    int i;
    char *person = NULL;
    int passedCommandType = 0;
    for(i=0; command[i] != '\0'; i++){
        if(passedCommandType && command[i] != ' ')
            person[i] = command[i];
        else if(command[i] == ' ' && passedCommandType == 0){
            passedCommandType = 1;
            continue;
        }
    }
    person[i] = '\0';
    for(i; i < strlen(command);i++){
        int j = 0;
        cmdline[2][j++] = command[i];
    }
    cmdline[2][i] = '\0';
    strcpy(cmdline[1], person);
    printf("%s: %s", cmdline[1], cmdline[2]);
}

void parseCommand(char *command){
    int i,j,k;

    j=0;k=0;
    for(i=0;i<=strlen(command);i++){
         
        if(command[i] == ' '){
            if(command[i+1] != ' '){
                cmdline[k][j] = '\0';
                j=0;
                k++;
            }
            continue;
        }
        else{
            //copy other characters
            cmdline[k][j++] = command[i];
        }       
    }
    cmdline[k][j]='\0';
}
