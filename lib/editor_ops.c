#include "editor_ops.h"


void edInsertChar(int c) {
  if (E.cY == E.nRows) {
    // add a new row if we're at the end
    edInsertRow(E.nRows, "", 0);
  }
  edRowInsertChar(&E.row[E.cY], E.cX, c);
  E.cX++;
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
