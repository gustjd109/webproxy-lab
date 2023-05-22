CC = gcc
CFLAGS = -O2 -Wall -I .
LIB = -lpthread

all: echo

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

echoserver: echoserver.c csapp.o
	$(CC) $(CFLAGS) -o echoserver echoserver.c csapp.o $(LIB)

clean:
	rm -f *.o echo *~
	rm -f *.o echoclient *~
	rm -f *.o echoserver *~