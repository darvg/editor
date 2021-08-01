#ifndef EDITOR_CONFIGS_H_
#define EDITOR_CONFIGS_H_

#include <termios.h>
#include <time.h>

#include "row.h"

struct edSyntax {
  char *fType;
  char **fMatch;
  char *commentStartToken;
  char *mcommentStartToken;
  char *mcommentEndToken;
  char **keywords;
  int flags;
};

struct edConfig {
  int cX, cY;
  int rX; // what is actually being rendered to the screen?
  int sRows;
  int sCols;
  int nRows;
  int rowOff;
  int colOff;
  int dirty; // is file changed?
  char *fname;
  char smsg[80];
  time_t smsgTime;
  struct edSyntax *syntax;
  edRow *row;
  struct termios orig_termios;
};

extern struct edConfig E;

#endif // EDITOR_CONFIGS_H_
