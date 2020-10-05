

CC=gcc
CFLAGS=-g -Wall

all: tictactoeServer tictactoeClient

tictactoeServer: tictactoeServer.c
	$(CC) $(CFLAGS) -o tictactoeServer tictactoeServer.c

tictactoeClient: tictactoeClient.c
	$(CC) $(CFLAGS) -o tictactoeClient tictactoeClient.c

clean:
	rm tictactoeServer
	rm tictactoeClient

