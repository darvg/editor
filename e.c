#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <fcntl.h>
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

#define HL_NUMBERS (1 << 0)
#define HL_STRINGS (1 << 1)

// map for special keys
enum specialKeys {
BACKSPACE = 127,
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

enum highlightVals {
HL_NORMAL = 0,
HL_STRING,
HL_COMMENT,
HL_MCOMMENT,
HL_KEYWORD1,
HL_KEYWORD2,
HL_NUMBER,
HL_SEARCH
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
  int rowInd;
  int hlOpenComment;
  char *render;
  char *chars;
  unsigned char *hl;
} edRow;

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
struct edConfig E;


/*** filetypes ***/
char *cExts[] = {".c", ".cpp", ".h", NULL};
char *cKeywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

char *pyExts[] = {".py", NULL};
char *pyKeywords[] = {
  "False", "None", "True", "and", "as", "assert", "async", "await",
  "break", "class", "continue", "def", "del", "elif", "else", "except",
  "finally", "for", "from", "global", "if", "import", "in", "is", "lambda",
  "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL
};

struct edSyntax HLDB[] = {
  {
    "c",
    cExts,
    "//", "/*", "*/",
    cKeywords,
    HL_NUMBERS | HL_STRINGS
  },
  
  {
    "python",
    pyExts,
    "#", "\"\"\"", "\"\"\"",
    pyKeywords,
    HL_NUMBERS | HL_STRINGS
  }
  
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

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
char *edPrompt(char *prompt, void (*callback)(char *, int));

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
void *edRowsToString(int *bufLen);
void edSave();

// row ops.
void edInsertRow(int a, char *s, size_t len);
void edUpdateRow(edRow *row); //help us handle tabs
void edDeleteRow(int at);
void edFreeRow(edRow *row);
int edComputeRx(edRow *row, int cX);
int edComputeCx(edRow *row, int rX);
void edRowInsertChar(edRow *row, int at, int c);
void edRowRemoveChar(edRow *row, int at);
void edRowAppendStr(edRow *row, char *s, size_t len);

// editor operations
void edInsertChar(int c);
void edRemoveChar();
void edInsertNewline();

// search
void edSearch();
void edSearchCallback(char *q, int k);

// syntax highlighting
void edUpdateHL(edRow *row);
int edSyntaxToColor(int hl);
void edChooseHL();
int isSep(int c);


/*** init ***/
void init_editor() {
  // init x,y coords of cursor w.r.t. terminal
  E.cX = 0;
  E.cY = 0;
  E.rX = 0;
  E.nRows = 0;
  E.rowOff = 0;
  E.colOff = 0;
  E.dirty = 0;
  E.row = NULL;
  E.fname = NULL;
  E.syntax = NULL;
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

  edSetSMessage("CTRL-S to save | CTRL-Q to quit | CTRL-F to search");

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
  static int confirm_quit = 1;

  switch (c) {
    case '\r':
      edInsertNewline();
      break;

    case CTRL_KEY('q'):
      if (E.dirty && confirm_quit) {
        edSetSMessage("File has unsaved changes. Press CTRL-Q again to discard them & quit.");
        confirm_quit = 0;
        return;
      }
      edClearScreen();
      exit(0);
      break;

    case CTRL_KEY('f'):
      edSearch();
      break;

    case CTRL_KEY('s'):
      edSave();
      break;

    case HOME_KEY:
      E.cX = 0;
      break;

    case END_KEY:
      if (E.cY < E.nRows) {
        E.cX = E.row[E.cY].size;
      }
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      edRemoveChar();
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

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      edInsertChar(c);
      break;
  }

  confirm_quit = 1; // this goes if any key other than ctrl-q is pressed.
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

void edOpen(char* fname) {
  free(E.fname);
  E.fname = strdup(fname);

  FILE* fp = fopen(fname, "r");
  if (!fp) error_exit("fopen");

  edChooseHL();

  char* line = NULL;
  size_t lineCap = 0;
  ssize_t lineLen;
  while ((lineLen = getline(&line, &lineCap, fp)) != -1) {
    while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r'))
      lineLen--;
    edInsertRow(E.nRows, line, lineLen);
  }

  free(line);
  fclose(fp);
  E.dirty = 0; // not actually dirty
}

void edUpdateRow(edRow *row) {
  // handles rendering the tabs based on the given chars
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

  edUpdateHL(row);
}

void edInsertRow(int a, char *s, size_t len) {
  if (a < 0 || a > E.nRows) return;

  E.row = realloc(E.row, sizeof(edRow) * (E.nRows + 1));
  memmove(&E.row[a + 1], &E.row[a], sizeof(edRow) * (E.nRows - a));

  // update each displaced row
  for (int j = a + 1; j <= E.nRows; j++) E.row[j].rowInd++;

  E.row[a].size = len;
  E.row[a].chars = malloc(len + 1);
  memcpy(E.row[a].chars, s, len);
  E.row[a].chars[len] = '\0';

  E.row[a].rSize = 0;
  E.row[a].render = NULL;
  E.row[a].hl = NULL;

  E.row[a].rowInd = a;
  E.row[a].hlOpenComment = 0;
  edUpdateRow(&E.row[a]);

  E.nRows++;
  E.dirty++;
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

int edComputeCx(edRow *row, int rX) {
  int rX_t = 0;
  int cX;
  for (cX = 0; cX < row->size; cX++) {
    if (row->chars[cX] == '\t')
      rX_t += (TAB_STOP - 1) - (rX_t % TAB_STOP);
    rX_t++;

    if (rX_t > rX) return cX;
  }
  return cX;
}

void edRowInsertChar(edRow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);

  // make space for the new char at spot at
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;

  // update render/rsize fields
  edUpdateRow(row);
  E.dirty++;
}

void edInsertChar(int c) {
  if (E.cY == E.nRows) {
    // add a new row if we're at the end
    edInsertRow(E.nRows, "", 0);
  }
  edRowInsertChar(&E.row[E.cY], E.cX, c);
  E.cX++;
}

void edRowRemoveChar(edRow *row, int at) {
  if (at < 0 || at >= row->size) return;

  // overwrite the char at index at
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  edUpdateRow(row);
  E.dirty++;
}

void edRemoveChar() {
  if (E.cY == E.nRows) return;
  if (E.cY == 0 && E.cX == 0) return;

  edRow *row = &E.row[E.cY];
  if (E.cX > 0) {
    edRowRemoveChar(row, E.cX - 1);
    E.cX--;
  } else {
    E.cX = E.row[E.cY - 1].size;
    edRowAppendStr(&E.row[E.cY - 1], row->chars, row->size);
    edDeleteRow(E.cY);
    E.cY--;
  }
}

void *edRowsToString(int *bufLen) {
  // get the length of all the rows
  int totalLen = 0;
  int j;
  for (j = 0; j < E.nRows; j++) {
    totalLen += E.row[j].size + 1;
  }
  *bufLen = totalLen;

  char *buf = malloc(totalLen);
  char *p = buf;
  for (j = 0; j < E.nRows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void edFreeRow(edRow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void edDeleteRow(int at) {
  if (at < 0 || at >= E.nRows) return;
  edFreeRow(&E.row[at]);

  // delete the current row, shift the rows under it up by 1
  memmove(&E.row[at], &E.row[at + 1], sizeof(edRow) * (E.nRows - at - 1));

  // update each displaced row
  for (int j = at; j < E.nRows - 1; j++) E.row[j].rowInd--;
  E.nRows--;
  E.dirty++;
}

void edSave() {
  if (E.fname == NULL) {
    E.fname = edPrompt("Save as (ESC to cancel): %s", NULL);
    if (E.fname == NULL) {
      edSetSMessage("Save aborted.");
      return;
    }
    edChooseHL();
  }

  int len;
  char *buf = edRowsToString(&len);

  int fd = open(E.fname, O_RDWR | O_CREAT, 0644);
  if (fd != 1) {
    if (ftruncate(fd, len) != 1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        edSetSMessage("%d bytes written", len);
        E.dirty = 0; // no longer dirty
        return;
      }
    }
    close(fd);
  }
  free(buf);
  edSetSMessage("save error: %s", strerror(errno));
}

void edRowAppendStr(edRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  edUpdateRow(row);
  E.dirty++;
}

void edInsertNewline() {
  if (E.cX == 0) {
    // when the cursor is at the start, create a row above
    edInsertRow(E.cY, "", 0);
  } else {
    edRow *row = &E.row[E.cY];

    // create a row under the current one, with space for all characters to the right
    edInsertRow(E.cY + 1, &row->chars[E.cX], row->size - E.cX);
    row = &E.row[E.cY]; // reassignment due to the possible realloc memory shuffle
    row->size = E.cX;
    row->chars[row->size] = '\0';
    edUpdateRow(row);
  }
  E.cY++;
  E.cX = 0;
}

char *edPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bSize = 128;
  char *buf = malloc(bSize);

  size_t bLen = 0;
  buf[0] = '\0';

  while (1) {
    // similar control loop to main as we fill out the user's answer to the prompt
    edSetSMessage(prompt, buf);
    edRefreshScreen();

    int c = edReadKey();
    if (c == BACKSPACE || c == DEL_KEY || c == CTRL_KEY('h')) {
      // delete current character
      if (bLen != 0)
        buf[--bLen] = '\0';
    } else if (c == '\x1b') {
      // esc. key to get out of answering prompt
      edSetSMessage("");
      free(buf);
      if (callback) callback(buf, c); // cancel the search
      return NULL;
    } else if (c == '\r') {
      // enter key to return user answer
      if (bLen != 0) {
        edSetSMessage("");
        if (callback) callback(buf, c); // exit the search
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      // add to user's running answer
      if (bLen == bSize - 1) {
        bSize *= 2;
        buf = realloc(buf, bSize);
      }
      buf[bLen++] = c;
      buf[bLen] = '\0';
    }

    // for incremental search (we search as the query is built)
    if (callback) callback(buf, c);
  }
}

void edSearchCallback(char *q, int k) {
  static int prevMatch = -1;
  static int direction = 1; // 1 for forward, -1 for backward

  static int savedHLLine;
  static char *savedHL = NULL;

  // undo the highlighting for a search query
  // guaranteed to be called since we use this function when leaving search mode
  if (savedHL) {
    memcpy(E.row[savedHLLine].hl, savedHL, E.row[savedHLLine].rSize);
    free(savedHL);
    savedHL = NULL;
  }

  // set up variables for moving through search results
  if (k == '\r' || k == '\x1b') {
    prevMatch = -1;
    direction = 1;
    return;
  } else if (k == ARROW_RIGHT || k == ARROW_DOWN) {
    direction = 1;
  } else if (k == ARROW_LEFT || k == ARROW_UP) {
    direction = -1;
  } else {
    prevMatch = -1;
    direction = 1;
  }

  if (prevMatch == -1) direction = 1; // we search forward by default
  int current = prevMatch;
  int i;

  // searches based on current, and current persists due to staticness.
  // leads to ness-incremental.
  for (i = 0; i < E.nRows; i++) {
    current += direction;
    if (current == -1) current = E.nRows - 1;
    else if (current == E.nRows) current = 0;

    edRow *row = &E.row[current];
    char *match = strstr(row->render, q);
    if (match) {
      prevMatch = current;
      E.cY = current;
      E.cX = edComputeCx(row, match - row->render);
      E.rowOff = E.nRows;

      savedHLLine = current;
      savedHL = malloc(row->rSize);
      memcpy(savedHL, row->hl, row->rSize);
      memset(&row->hl[match - row->render], HL_SEARCH, strlen(q));
      break;
    }
  }
}

void edSearch() {
  int cX_t = E.cX;
  int cY_t = E.cY;
  int colOff_t = E.colOff;
  int rowOff_t = E.rowOff;

  char *q = edPrompt("Search token (ESC/ENTER/Arrows to navigate): %s", edSearchCallback);

  if (q) {
    free(q);
  } else {
    // we chose to not use the query => query is null, so we restore former position
    E.cX = cX_t;
    E.cY = cY_t;
    E.colOff = colOff_t;
    E.rowOff = rowOff_t;
  }
}

int isSep(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

int edSyntaxToColor(int hl) {
  switch(hl) {
    case HL_NUMBER: return 31;
    case HL_KEYWORD1: return 33;
    case HL_KEYWORD2: return 32;
    case HL_SEARCH: return 34;
    case HL_STRING: return 35;
    case HL_COMMENT:
    case HL_MCOMMENT: return 90;
    default: return 37;
  }
}

void edUpdateHL(edRow *row) {
  row->hl = realloc(row->hl, row->rSize);
  memset(row->hl, HL_NORMAL, row->rSize);

  if (E.syntax == NULL) return;

  char **keywords = E.syntax->keywords;

  // comment tokens
  char *cst = E.syntax->commentStartToken;
  char *mcst = E.syntax->mcommentStartToken;
  char *mcet = E.syntax->mcommentEndToken;

  // comment token lens
  int cstLen = cst ? strlen(cst) : 0;
  int mcstLen = mcst ? strlen(mcst) : 0;
  int mcetLen = mcet ? strlen(mcet) : 0;

  int prevSep = 1;
  int inStr = 0;
  int inComment = (row->rowInd > 0 && E.row[row->rowInd - 1].hlOpenComment);

  int i = 0;
  while (i < row->rSize) {
    char c = row->render[i];
    unsigned char prevHL = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    // hl single-line comments
    if (cstLen && !inStr && !inComment) {
      if (!strncmp(&row->render[i], cst, cstLen)) {
        memset(&row->hl[i], HL_COMMENT, row->rSize - i);
        break;
      }
    }

    // hl multi-line comments
    if (mcstLen && mcetLen && !inStr) {
      if (inComment) {
        // if we're in a multiline comment, then we can safely highlight
        row->hl[i] = HL_MCOMMENT;
        if (!strncmp(&row->render[i], mcet, mcetLen)) {
          memset(&row->hl[i], HL_MCOMMENT, mcetLen);
          i += mcetLen;
          inComment = 0;
          prevSep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcst, mcstLen)) {
        // check if the current token is the start of a multiline comment
        memset(&row->hl[i], HL_MCOMMENT, mcstLen);
        i += mcstLen;
        inComment = 1;
        continue;
      }
    }

    // hl strings
    if (E.syntax->flags & HL_STRINGS) {
      if (inStr) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rSize) {
          // handling escaped quotes
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == inStr) inStr = 0; // once we see another "/', we exit string mode
        i++;
        prevSep = 1;
        continue;
      } else {
        // if we see a " or ', we assume we're in a string.
        if (c == '"' || c == '\'') {
          inStr = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    // make sure we need to highlight numbers for this file
    if (E.syntax->flags & HL_NUMBERS) {
      // prev. char must be num. or sep. for curr. num. to be highlighted
      // second case handles decimals
      if((isdigit(c) && (prevSep || prevHL == HL_NUMBER)) ||
         (c == '.' && prevHL == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;

        // we're in the middle of highlighting a sequence, so we increment/continue
        i++;
        prevSep = 0;
        continue;
      }
    }

    // hl keywords
    if (prevSep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int kLen = strlen(keywords[j]);
        int kw2 = keywords[j][kLen - 1] == '|';
        if (kw2) kLen--;

        if (!strncmp(&row->render[i], keywords[j], kLen) &&
            isSep(row->render[i + kLen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, kLen);
          i += kLen;
          break;
        }
      }

      if (keywords[j] == NULL) {
        prevSep = 0;
        continue;
      }
    }

    prevSep = isSep(c);
    i++;
  }

  // check if we're still in a ml comment or not
  int changed = (row->hlOpenComment != inComment);
  row->hlOpenComment = inComment;

  // if we are not, change the highlighting of the next line
  if (changed && row->rowInd + 1 < E.nRows)
    edUpdateHL(&E.row[row->rowInd + 1]);
}

void edChooseHL() {
  E.syntax = NULL;
  if (E.fname == NULL) return;

  char *ext = strrchr(E.fname, '.');

  // figure out which highlighting scheme to use based on HLDB
  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct edSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->fMatch[i]) {
      int isExt = (s->fMatch[i][0] == '.');
      if ((isExt && ext && !strcmp(ext, s->fMatch[i])) ||
          (!isExt && strstr(E.fname, s->fMatch[i]))) {
        E.syntax = s;

        // change the higlighting when the ftype changes
        int fRow;
        for (fRow = 0; fRow < E.nRows; fRow++) {
          edUpdateHL(&E.row[fRow]);
        }
        return;
      }
      i++;
    }
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
