#include "row_operations.h"

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

void edRowRemoveChar(edRow *row, int at) {
  if (at < 0 || at >= row->size) return;

  // overwrite the char at index at
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  edUpdateRow(row);
  E.dirty++;
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

void edRowAppendStr(edRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  edUpdateRow(row);
  E.dirty++;
}
