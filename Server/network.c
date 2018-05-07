#include "server.h"

sqlite3 *db;
GROUP *head = NULL;

int main (int argc, char *argv[])
{
  signal(SIGINT, exit_nicely);     //call exit_nicely when the user close the program
  set_priority();                  //setting the priority for this program
  init_database();                 //opening a database for later use

  int port;                        //check if user has specified a port to use
  if(argc == 2){
    port = atoi(argv[1]);
  } else {
    port = PORT;
  }

  int local_sock = init_socket(port); //setting up the socket and port to listen
  fprintf(stderr,"[Socket] %d\n[Port] %d\n", local_sock, port);

  time_t t = time(NULL);

  for(;;){
    open_stuff(local_sock);       //the server is listening for connection and accept
    write_stuff();                //flush output buffer to user
    read_stuff();                 //getting user input
    process_stuff();              //process the user input
    remove_stuff();               //remove anyone with ST_BAD status
    usleep(10000);                //put the program to sleep to decrease cpu usage
    if(time(NULL) - t >= 30){
      t = time(NULL);
      fprintf(stderr,"[%d users connected]\n", count_users());
    }
  }
}
