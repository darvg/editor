#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

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


/*** func headers ***/

// terminal headers
void error_exit(const char* s);
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

/*** dynamic str buffer ***/
typedef struct dbuf {
  char *b;
  int len;
} str;

void dbAppend(str *db, const char *s, int len);
void dbFree(str *db);

/*** globals ***/
struct edConfig {
  int cX, cY;
  int sRows;
  int sCols;
  struct termios orig_termios;
};
struct edConfig E;

/*** init ***/
void init_editor() {
  // init x,y coords of cursor w.r.t. terminal
  E.cX = 0;
  E.cY = 0;

  if (getWindowSize(&E.sRows, &E.sCols) == -1)
    error_exit("getWindowSize");
}

int main() {
  enableRawMode();
  init_editor();

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
  switch (c) {
    case ARROW_LEFT:
      if (E.cX != 0) {
        E.cX--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cX != E.sCols - 1) {
        E.cX++;
      }
      break;
    case ARROW_UP:
      if (E.cY != 0) {
        E.cY--;
      }
      break;
    case ARROW_DOWN:
      if (E.cY != E.sRows - 1) {
        E.cY++;
      }
      break;
  }
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
      E.cX = E.sCols - 1;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
    {
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
  str db = ABUF_INIT;

  // empty screen and reposition cursor
  dbAppend(&db, "\x1b[?25l", 6);
  dbAppend(&db, "\x1b[H", 3);

  edDrawRows(&db);

  // let the user cursor stay where it is
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cY + 1, E.cX + 1);
  dbAppend(&db, buf, strlen(buf));

  // hide cursor during repaint
  dbAppend(&db, "\x1b[?25h", 6);

  // a mere one write to refresh the screen.
  write(STDOUT_FILENO, db.b, db.len);
  dbFree(&db);
}

void edDrawRows(str *db) {
  int y;
  for (y = 0; y < E.sRows; y++) {
    dbAppend(db, "~", 1);

    // erase as we go
    dbAppend(db, "\x1b[K", 3);

    // last row causes scrolling to get the "\r\n"
    if (y < E.sRows - 1)
      dbAppend(db, "\r\n", 2);
  }
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
