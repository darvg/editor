#include "editor_configs.h"
#include "file_io.h"
#include "terminal_config.h"

/*** globals ***/
struct edConfig E;

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
