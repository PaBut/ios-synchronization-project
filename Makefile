CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread -lrt

proj2: proj2.c
	$(CC) $(CFLAGS) proj2.c -o proj2