smallsh: smallsh.c shellfunc.c shellfunc.h
	gcc -o smallsh smallsh.c shellfunc.c -Wall

clean:
	rm -f smallsh

all:	smallsh
