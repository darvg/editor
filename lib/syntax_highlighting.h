#ifndef SYNTAX_HIGHLIGHTING_H_
#define SYNTAX_HIGHLIGHTING_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "constants.h"
#include "row.h"
#include "editor_configs.h"


void edUpdateHL(edRow *row);
int edSyntaxToColor(int hl);
void edChooseHL();
int isSep(int c);

#endif // SYNTAX_HIGHLIGHTING_H_
