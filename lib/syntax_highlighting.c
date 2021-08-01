#include "syntax_highlighting.h"

/*** filetypes ***/
char *cExts[] = {".c", ".cpp", ".h", NULL};
char *cKeywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

char *pyExts[] = {".py", NULL};
char *pyKeywords[] = {
  "False", "None", "True", "and", "as", "assert", "async", "await",
  "break", "class", "continue", "def", "del", "elif", "else", "except",
  "finally", "for", "from", "global", "if", "import", "in", "is", "lambda",
  "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL
};

struct edSyntax HLDB[] = {
  {
    "c",
    cExts,
    "//", "/*", "*/",
    cKeywords,
    HL_NUMBERS | HL_STRINGS
  },

  {
    "python",
    pyExts,
    "#", "\"\"\"", "\"\"\"",
    pyKeywords,
    HL_NUMBERS | HL_STRINGS
  }

};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

int isSep(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

int edSyntaxToColor(int hl) {
  switch(hl) {
    case HL_NUMBER: return 31;
    case HL_KEYWORD1: return 33;
    case HL_KEYWORD2: return 32;
    case HL_SEARCH: return 34;
    case HL_STRING: return 35;
    case HL_COMMENT:
    case HL_MCOMMENT: return 90;
    default: return 37;
  }
}

void edUpdateHL(edRow *row) {
  row->hl = realloc(row->hl, row->rSize);
  memset(row->hl, HL_NORMAL, row->rSize);

  if (E.syntax == NULL) return;

  char **keywords = E.syntax->keywords;

  // comment tokens
  char *cst = E.syntax->commentStartToken;
  char *mcst = E.syntax->mcommentStartToken;
  char *mcet = E.syntax->mcommentEndToken;

  // comment token lens
  int cstLen = cst ? strlen(cst) : 0;
  int mcstLen = mcst ? strlen(mcst) : 0;
  int mcetLen = mcet ? strlen(mcet) : 0;

  int prevSep = 1;
  int inStr = 0;
  int inComment = (row->rowInd > 0 && E.row[row->rowInd - 1].hlOpenComment);

  int i = 0;
  while (i < row->rSize) {
    char c = row->render[i];
    unsigned char prevHL = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    // hl single-line comments
    if (cstLen && !inStr && !inComment) {
      if (!strncmp(&row->render[i], cst, cstLen)) {
        memset(&row->hl[i], HL_COMMENT, row->rSize - i);
        break;
      }
    }

    // hl multi-line comments
    if (mcstLen && mcetLen && !inStr) {
      if (inComment) {
        // if we're in a multiline comment, then we can safely highlight
        row->hl[i] = HL_MCOMMENT;
        if (!strncmp(&row->render[i], mcet, mcetLen)) {
          memset(&row->hl[i], HL_MCOMMENT, mcetLen);
          i += mcetLen;
          inComment = 0;
          prevSep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcst, mcstLen)) {
        // check if the current token is the start of a multiline comment
        memset(&row->hl[i], HL_MCOMMENT, mcstLen);
        i += mcstLen;
        inComment = 1;
        continue;
      }
    }

    // hl strings
    if (E.syntax->flags & HL_STRINGS) {
      if (inStr) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rSize) {
          // handling escaped quotes
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == inStr) inStr = 0; // once we see another "/', we exit string mode
        i++;
        prevSep = 1;
        continue;
      } else {
        // if we see a " or ', we assume we're in a string.
        if (c == '"' || c == '\'') {
          inStr = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    // make sure we need to highlight numbers for this file
    if (E.syntax->flags & HL_NUMBERS) {
      // prev. char must be num. or sep. for curr. num. to be highlighted
      // second case handles decimals
      if((isdigit(c) && (prevSep || prevHL == HL_NUMBER)) ||
         (c == '.' && prevHL == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;

        // we're in the middle of highlighting a sequence, so we increment/continue
        i++;
        prevSep = 0;
        continue;
      }
    }

    // hl keywords
    if (prevSep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int kLen = strlen(keywords[j]);
        int kw2 = keywords[j][kLen - 1] == '|';
        if (kw2) kLen--;

        if (!strncmp(&row->render[i], keywords[j], kLen) &&
            isSep(row->render[i + kLen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, kLen);
          i += kLen;
          break;
        }
      }

      if (keywords[j] == NULL) {
        prevSep = 0;
        continue;
      }
    }

    prevSep = isSep(c);
    i++;
  }

  // check if we're still in a ml comment or not
  int changed = (row->hlOpenComment != inComment);
  row->hlOpenComment = inComment;

  // if we are not, change the highlighting of the next line
  if (changed && row->rowInd + 1 < E.nRows)
    edUpdateHL(&E.row[row->rowInd + 1]);
}

void edChooseHL() {
  E.syntax = NULL;
  if (E.fname == NULL) return;

  char *ext = strrchr(E.fname, '.');

  // figure out which highlighting scheme to use based on HLDB
  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct edSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->fMatch[i]) {
      int isExt = (s->fMatch[i][0] == '.');
      if ((isExt && ext && !strcmp(ext, s->fMatch[i])) ||
          (!isExt && strstr(E.fname, s->fMatch[i]))) {
        E.syntax = s;

        // change the higlighting when the ftype changes
        int fRow;
        for (fRow = 0; fRow < E.nRows; fRow++) {
          edUpdateHL(&E.row[fRow]);
        }
        return;
      }
      i++;
    }
  }
}
