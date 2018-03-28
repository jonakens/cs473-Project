
CC = gcc
CFLAGS = -g
LIBS = -lcrypt
OBJS = network.o application.o authenticate.o commands.o

server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
network.o: network.c server.h
	$(CC) $(CFLAGS) -c network.c
application.o: application.c server.h
	$(CC) $(CFLAGS) -c application.c
authenticate.o: authenticate.c server.h
	$(CC) $(CFLAGS) -c authenticate.c
commands.o: commands.c server.h
	$(CC) $(CFLAGS) -c commands.c
clean:
	rm -f *.o server
