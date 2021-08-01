#include "editor_output.h"

void edClearScreen() {
  //NOTE: using VT100 escape sequences. Refer to ncurses for more compatibility.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void edRefreshScreen() {
  edScroll();
  str db = ABUF_INIT;

  // empty screen and reposition cursor
  dbAppend(&db, "\x1b[?25l", 6);
  dbAppend(&db, "\x1b[H", 3);

  edDrawRows(&db);
  edStatusBar(&db);
  edMsgBar(&db);

  // update the cursor position based on keystrokes.
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cY - E.rowOff) + 1, (E.rX - E.colOff) + 1);
  dbAppend(&db, buf, strlen(buf));

  // hide cursor during repaint to prevent flickering
  dbAppend(&db, "\x1b[?25h", 6);

  // a mere one write to refresh the screen.
  write(STDOUT_FILENO, db.b, db.len);
  dbFree(&db);
}

void edDrawRows(str *db) {
  int y;
  for (y = 0; y < E.sRows; y++) {
    // based on the total number of rows in the file, we print '~'.
    // the row offset determines which part of the file we show
    int fRow = y + E.rowOff;
    if (fRow >= E.nRows) {
      dbAppend(db, "~", 1);
    } else {
      int len = E.row[fRow].rSize - E.colOff;
      if (len < 0) len = 0; // user can't go past end of line
      if (len > E.sCols) len = E.sCols; // user can't go past EOF

      // cutoff the starting part of the string that shouldn't be shown.
      char *preColor = &E.row[fRow].render[E.colOff];
      unsigned char *hl = &E.row[fRow].hl[E.colOff];
      int currColor = -1; // the default, i.e. white-on-black
      for (int j = 0; j < len; j++) {
        if (iscntrl(preColor[j])) {
          // represent unprintable characters
          char sym = (preColor[j] <= 26) ? '@' + preColor[j] : '?';
          dbAppend(db, "\x1b[7m", 4);
          dbAppend(db, &sym, 1);
          dbAppend(db, "\x1b[m", 3);
          if (currColor != -1) {
            // restore previous highlighting scheme
            char buf[16];
            int cLen = snprintf(buf, sizeof(buf), "\x1b[%dm", currColor);
            dbAppend(db, buf, cLen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (currColor != -1) {
            // set current color back to default
            dbAppend(db, "\x1b[39m", 5);
            currColor = -1;
          }
          dbAppend(db, &preColor[j], 1);
        } else {
          int c = edSyntaxToColor(hl[j]);
          if (c != currColor) {
            // set current color to new color
            currColor = c;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", c);
            dbAppend(db, buf, clen);
          }
          dbAppend(db, &preColor[j], 1);
        }
      }
      // make sure default is back for subsequent rows
      dbAppend(db, "\x1b[39m", 5);
    }

    // erase as we go
    dbAppend(db, "\x1b[K", 3);

    // add the newline because of the status bar.
    dbAppend(db, "\r\n", 2);
  }
}

void edStatusBar(str *db) {
  //'7m' switches to inverted colors (white on black), 'm' switches back
  dbAppend(db, "\x1b[7m", 4);
  char status[80], rStatus[80];

  // fname/total lines
  int len = snprintf(status, sizeof(status), "%20s - %d lines %s",
                     E.fname ? E.fname : "[No Name]", E.nRows,
                     E.dirty ? "(modified)" : "");

  // current line
  int rLen = snprintf(rStatus, sizeof(rStatus), "%s | %d/%d",
                      E.syntax ? E.syntax->fType : "no ft", E.cY + 1, E.nRows);

  if (len > E.sCols) len = E.sCols;
  dbAppend(db, status, len);

  while (len < E.sCols) {
    // only put currnet line number if there's space
    if (E.sCols - len == rLen) {
      dbAppend(db, rStatus, rLen);
      break;
    } else {
      dbAppend(db, " ", 1);
      len++;
    }
  }

  dbAppend(db, "\x1b[m", 3);
  dbAppend(db, "\r\n", 2);
}

void edSetSMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.smsg, sizeof(E.smsg), fmt, ap);
  va_end(ap);

  E.smsgTime = time(NULL);
}

void edMsgBar(str *db) {
  dbAppend(db, "\x1b[K", 3);
  int msgLen = strlen(E.smsg);
  if (msgLen > E.sCols) msgLen = E.sCols;
  if (msgLen && time(NULL) - E.smsgTime < 5)
    dbAppend(db, E.smsg, msgLen);
}

void edScroll() {
  E.rX = 0;

  // compute rX
  if (E.cY < E.nRows) {
    E.rX = edComputeRx(&E.row[E.cY], E.cX);
  }

  // cursor is above
  if (E.cY < E.rowOff) {
    E.rowOff = E.cY;
  }

  // cursor is below
  if (E.cY >= E.rowOff + E.sRows) {
    E.rowOff = E.cY - E.sRows + 1;
  }

  // cursor is left
  if (E.cX < E.colOff) {
    E.colOff = E.rX;
  }

  // cursor is right
  if (E.rX > E.colOff + E.sCols) {
    E.colOff = E.rX - E.sCols + 1;
  }
}
