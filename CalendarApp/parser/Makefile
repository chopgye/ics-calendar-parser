#Dorjee Tenzin
#0980180
#Assignment 2 

CC = gcc
CFLAGS = -Wall -g -std=c11 -Iinclude/

all: list parser

list: src/LinkedListAPI.c
	$(CC) -c -fPIC $(CFLAGS) src/LinkedListAPI.c -o bin/list.o
	$(CC) -shared -o ../liblist.so bin/list.o
parser: src/CalendarParser.c list
	$(CC) -c -fPIC $(CFLAGS) src/CalendarParser.c -o bin/parser.o
	$(CC) -shared -o ../libcal.so bin/parser.o bin/list.o
clean:
	rm bin/*.o
	rm bin/*.so