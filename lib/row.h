#ifndef ROW_H_
#define ROW_H_

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

#endif // ROW_H_
