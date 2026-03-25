CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -pedantic

.PHONY: all clean

all:
	$(CC) $(CFLAGS) -o process sigtrap.c
	$(CC) $(CFLAGS) -o question1 question1.c
	$(CC) $(CFLAGS) -o question2 question2.c

clean:
	rm -f process question1 question2