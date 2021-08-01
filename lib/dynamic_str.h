#ifndef DYNAMIC_STR_H_
#define DYNAMIC_STR_H_

#include <stdlib.h>
#include <string.h>

/*** dynamic str buffer ***/
typedef struct dbuf {
  char *b;
  int len;
} str;

void dbAppend(str *db, const char *s, int len);
void dbFree(str *db);

#endif // DYNAMIC_STR_H_
