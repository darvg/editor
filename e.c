#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** func headers ***/

// terminal headers
void error_exit(const char* s);
void enableRawMode();
void disableRawMode();
char edReadKey();
int getWindowSize(int *rows, int *cols);

// input headers
void edProcessStroke();

// output headers
void edClearScreen();
void edRefreshScreen();
void edDrawRows();


/*** global vars ***/
struct edConfig {
  int sRows;
  int sCols;
  struct termios orig_termios;
};
struct edConfig E;

/*** init ***/
void init_editor() {
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

char edReadKey() {
  int r;
  char c;
  while ((r = read(STDIN_FILENO, &c, sizeof(char))) != 1) {
    if (r == -1 && errno != EAGAIN)
      error_exit("read");
  }
  return c;
}

void edProcessStroke() {
  char c = edReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      edClearScreen();
      exit(0);
      break;
  }
}

void edClearScreen() {
  //NOTE: using VT100 escape sequences. Refer to ncurses for more compatibility.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void edRefreshScreen() {
  edClearScreen();
  edDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

void edDrawRows() {
  int y;
  for (y = 0; y <= 24; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
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
