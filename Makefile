sunsche : main.o
	gcc -lm -o sunsche main.o
main.o : main.c
	gcc -c main.c init.h
