#include "dynamic_str.h"

void dbAppend(str *db, const char *s, int len) {
  char *new = realloc(db->b, db->len + len);

  if (new == NULL) return;
  memcpy(&new[db->len], s, len);
  db->b = new;
  db->len += len;
}

void dbFree(str *db) {
  free(db->b);
}
