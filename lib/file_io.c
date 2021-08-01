#include "file_io.h"


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
