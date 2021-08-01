#ifndef EDITOR_INPUT_H_
#define EDITOR_INPUT_H_


#include "constants.h"
#include "editor_configs.h"
#include "editor_ops.h"
#include "editor_search.h"
#include "file_io.h"
#include "row.h"
#include "terminal_config.h"

void edProcessStroke();
void edMoveCursor(int c);
char *edPrompt(char *prompt, void (*callback)(char *, int));

#endif // EDITOR_INPUT_H_
