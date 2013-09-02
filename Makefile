all:
	gcc -g -c main.c
	gcc -g main.o -lm -lbladeRF -lpthread -o radar
