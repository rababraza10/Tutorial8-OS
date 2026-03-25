.PHONY: all clean

all:
	gcc sigtrap.c -o process
	gcc question1.c -o question1
	gcc question2.c -o question2

clean:
	rm -f process question1 question2