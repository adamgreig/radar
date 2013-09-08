all:
	gcc -O3 -Wall -c main.c
	gcc -O3 -Wall main.o -lm -lbladeRF -lpthread -o radar
