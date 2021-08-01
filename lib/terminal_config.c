#include "terminal_config.h"

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
