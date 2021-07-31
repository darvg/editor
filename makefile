e: e.c
	$(CC) e.c -Wall -Wextra -pedantic -std=c99 -o e

debug:
	$(CC) e.c -Wall -Wextra -pedantic -std=c99 -o e -g

clean:
	rm -rf e
