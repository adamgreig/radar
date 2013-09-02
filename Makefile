all:
	gcc -c main.c
	gcc main.o -lm -lbladeRF -o radar
