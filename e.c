#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}
#define TAB_STOP 8

// map for arrow keys
enum arrows {
ARROW_LEFT = 1000,
ARROW_RIGHT,
ARROW_UP,
ARROW_DOWN,
HOME_KEY,
END_KEY,
DEL_KEY,
PAGE_UP,
PAGE_DOWN
};


/*** dynamic str buffer ***/
typedef struct dbuf {
  char *b;
  int len;
} str;

void dbAppend(str *db, const char *s, int len);
void dbFree(str *db);


/*** globals ***/
// represents a row of text in a file to be displayed.
typedef struct edRow {
  int size;
  int rSize;
  char *render;
  char *chars;
} edRow;

struct edConfig {
  int cX, cY;
  int rX; // what is actually being rendered to the screen?
  int sRows;
  int sCols;
  int nRows;
  int rowOff;
  int colOff;
  char *fname;
  char smsg[80];
  time_t smsgTime;
  edRow *row;
  struct termios orig_termios;
};
struct edConfig E;


/*** func headers ***/
// terminal headers
void error_exit(const char *s);
void enableRawMode();
void disableRawMode();
int edReadKey();
int getWindowSize(int *rows, int *cols);

// input headers
void edProcessStroke();
void edMoveCursor(int c);

// output headers
void edClearScreen();
void edRefreshScreen();
void edDrawRows();
void edScroll();
void edStatusBar(str *db);
void edSetSMessage(const char *fmt, ...);
void edMsgBar(str *db);

// file I/O
void edOpen(char *fname);

// row ops.
void edAppendRow(char *s, size_t len);
void edUpdateRow(edRow *row); //help us handle tabs
int edComputeRx(edRow *row, int cX);


/*** init ***/
void init_editor() {
  // init x,y coords of cursor w.r.t. terminal
  E.cX = 0;
  E.cY = 0;
  E.rX = 0;
  E.nRows = 0;
  E.rowOff = 0;
  E.colOff = 0;
  E.row = NULL;
  E.fname = NULL;
  E.smsg[0] = '\0';
  E.smsgTime = 0;

  if (getWindowSize(&E.sRows, &E.sCols) == -1)
    error_exit("getWindowSize");

  // free a line up at the bottom for the status bar
  E.sRows -= 2;
}

int main(int argc, char* argv[]) {
  enableRawMode();
  init_editor();
  if (argc >= 2) {
    edOpen(argv[1]);
  }

  edSetSMessage("CTRL-Q to quit.");

  while (1) {
    edRefreshScreen();
    edProcessStroke();
  }
  return 0;
}


/*** func implementations ***/
void error_exit(const char* s) {
  edClearScreen();

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    error_exit("tcsetattr");
}

void enableRawMode() {
  if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    error_exit("tcgetattr");

  // when we terminate the program, run disableRawMode()
  atexit(disableRawMode);


  struct termios raw = E.orig_termios;

  /*
  ** termios.c_iflag contains input modes for the term. state
  ** BRKINT - archaic, but tradition
  ** INPCK - archaic, but tradition
  ** ISTRIP - archaic, but tradition
  ** ICRNL - stop translating '\r' into '\n'
  ** IXON - pause flow control (e.g. from ctrl-s/ctrl-q)
  */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  /*
  ** termios.c_oflag contains output modes for the term. state
  ** OPOST - similar to ICRNL, but for outputs
  */
  raw.c_oflag &= ~OPOST;

  /*
  ** termios.c_cflag contains control modes for the term. state
  ** CS8 - bit mask setting char size to 8 bits
  */
  raw.c_cflag |= ~CS8;

  /*
  ** termios.c_lflag contains misc. modes for the term. state
  ** ECHO - all keystrokes are printed to the terminal
  ** ICANON - enables canonical
  ** IEXTEN - not sure, but something to do with ctrl-v?
  ** ISIG - prevent SIGINT/SIGTSTP
  */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // introduce timeouts on read
  raw.c_cc[VMIN] = 0; // # bytes before returning
  raw.c_cc[VTIME] = 1; // 100 ms before read returns

  // TCSAFLUSH means set only after all output is written to the terminal
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    error_exit("tcsetattr");
}

int edReadKey() {
  int r;
  char c;
  while ((r = read(STDIN_FILENO, &c, sizeof(char))) != 1) {
    if (r == -1 && errno != EAGAIN)
      error_exit("read");
  }

  if (c == '\x1b') {
    char seq[3];

    // check to see if esc. sequence or just esc.
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

void edMoveCursor(int c) {
  edRow *row = (E.cY >= E.nRows) ? NULL : &E.row[E.cY];

  switch (c) {
    case ARROW_LEFT:
      if (E.cX != 0) {
        E.cX--;
      // move to end of previous line
      } else if (E.cY > 0) {
        E.cY--;
        E.cX = E.row[E.cY].size;
      }
      break;
    case ARROW_RIGHT:
      //unlimited right scroll not allowed
      if (row && E.cX < row->size) {
        E.cX++;
      // move ot start of next line
      } else if (row && E.cX == row->size) {
        E.cY++;
        E.cX = 0;
      }
      break;
    case ARROW_UP:
      if (E.cY != 0) {
        E.cY--;
      }
      break;
    case ARROW_DOWN:
      if (E.cY < E.nRows) {
        E.cY++;
      }
      break;
  }

  // snap cursor to end of row if curr. row is shorter than prev. row
  row = (E.cY >= E.nRows) ? NULL : &E.row[E.cY];
  int rowLen = row ? row->size : 0;
  if (E.cX > rowLen)
    E.cX = rowLen;
}

void edProcessStroke() {
  int c = edReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      edClearScreen();
      exit(0);
      break;

    case HOME_KEY:
      E.cX = 0;
      break;

    case END_KEY:
      if (E.cY < E.nRows) {
        E.cX = E.row[E.cY].size;
      }
      break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
      //move cursor to top/bottom, then simulate screen's worth of ups/downs
      if (c == PAGE_UP) {
        E.cY = E.rowOff;
      } else if (c == PAGE_DOWN) {
        E.cY = E.rowOff + E.sRows - 1;
        if (E.cY > E.nRows) E.cY = E.nRows;
      }

      int times = E.sRows;
      while (times--)
        edMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      break;
    }

    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
      edMoveCursor(c);
      break;
  }
}

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
      dbAppend(db, &E.row[fRow].render[E.colOff], len);
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
  int len = snprintf(status, sizeof(status), "%20s - %d lines",
                     E.fname ? E.fname : "[No Name]", E.nRows);

  // current line
  int rLen = snprintf(rStatus, sizeof(rStatus), "%d/%d", E.cY + 1, E.nRows);

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

void edOpen(char* fname) {
  free(E.fname);
  E.fname = strdup(fname);

  FILE* fp = fopen(fname, "r");
  if (!fp) error_exit("fopen");

  char* line = NULL;
  size_t lineCap = 0;
  ssize_t lineLen;
  while ((lineLen = getline(&line, &lineCap, fp)) != -1) {
    while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r'))
      lineLen--;
    edAppendRow(line, lineLen);
  }

  free(line);
  fclose(fp);
}

void edUpdateRow(edRow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);

  // row->size counts 1 per tab, so multiply tabs by 7 (assumes 8 spaces per tab)
  row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);

  int i = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[i++] = ' ';
      while (i % TAB_STOP != 0) row->render[i++] = ' ';
    } else {
      row->render[i++] = row->chars[j];
    }
  }

  row->render[i] = '\0';
  row->rSize = i;
}

void edAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(edRow) * (E.nRows + 1));

  int a = E.nRows;
  E.row[a].size = len;
  E.row[a].chars = malloc(len + 1);
  memcpy(E.row[a].chars, s, len);
  E.row[a].chars[len] = '\0';

  E.row[a].rSize = 0;
  E.row[a].render = NULL;
  edUpdateRow(&E.row[a]);

  E.nRows++;
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

int edComputeRx(edRow *row, int cX) {
  int rX = 0;
  int j;
  for (j = 0; j < cX; j++) {
    if (row->chars[j] == '\t') {
      //# cols to the left we are from the next tab_stop
      rX += (TAB_STOP - 1) - (rX % TAB_STOP);
    }
    rX++; //puts us right on top of the next tab_stop
  }

  return rX;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  // we use ioctl to capture the window size of the output vector
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

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
