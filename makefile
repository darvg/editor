e: e.c
	$(CC) e.c -Wall -Wextra -pedantic -std=c99 -o e

clean:
	rm -rf e
