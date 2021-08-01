#ifndef EDITOR_OUTPUT_H_
#define EDITOR_OUTPUT_H_

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "constants.h"
#include "dynamic_str.h"
#include "editor_configs.h"
#include "row_operations.h"
#include "syntax_highlighting.h"


void edClearScreen();
void edRefreshScreen();
void edDrawRows();
void edScroll();
void edStatusBar(str *db);
void edSetSMessage(const char *fmt, ...);
void edMsgBar(str *db);


#endif // EDITOR_OUTPUT_H_
