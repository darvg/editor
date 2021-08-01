#ifndef ROW_OPERATIONS_H_
#define ROW_OPERATIONS_H_

#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "row.h"
#include "syntax_highlighting.h"


void edInsertRow(int a, char *s, size_t len);
void edUpdateRow(edRow *row); //help us handle tabs
void edDeleteRow(int at);
void edFreeRow(edRow *row);
int edComputeRx(edRow *row, int cX);
int edComputeCx(edRow *row, int rX);
void edRowInsertChar(edRow *row, int at, int c);
void edRowRemoveChar(edRow *row, int at);
void edRowAppendStr(edRow *row, char *s, size_t len);

#endif // ROW_OPERATIONS_H_
