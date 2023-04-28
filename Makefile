CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread -lrt

deadlock: proj2		
	@./deadlock.sh 5 3 0 0 0
    @./deadlock.sh 1 3 10 10 0
    @./deadlock.sh 3 2 100 100 100
    @./deadlock.sh 33 22 100 100 1000
	@./deadlock.sh 100 100 100 100 1000
proj2: proj2.c
	$(CC) $(CFLAGS) proj2.c -o proj2