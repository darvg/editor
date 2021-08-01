#include "editor_search.h"

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
