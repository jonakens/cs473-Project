#include "server.h"

sqlite3 *db;
GROUP *head = NULL;

int main(int argc, char *argv[])
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

  int s;                           //setting up the socket and port to listen
  s = init_socket(port);
  fprintf(stderr,"[Socket] %d\n[Port] %d\n", s, port);

  time_t t = time(0);

  for(;;){
    open_stuff(s);                 //the server is listening for connection and accept
    write_stuff(s);                //flush output buffer to user
    read_stuff(s);                 //getting user input
    process_stuff(s);              //process the user input
    remove_stuff(s);               //remove anyone with ST_BAD status
    usleep(10000);                 //put the program to sleep to decrease cpu usage
    if(time(0) - t >= 30){
      t = time(0);
      fprintf(stderr,"[%d users connected]\n", count_users());
    }
  }
}
