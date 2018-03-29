CC=gcc
LIBS= `pkg-config --libs gtk+-3.0` -lGL -lGLU -lglut
CFLAGS=`pkg-config --cflags gtk+-3.0`

gtk: main.c
	$(CC) $(CFLAGS) -o gtk main.c $(LIBS)

