#include "editor_input.h"

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
