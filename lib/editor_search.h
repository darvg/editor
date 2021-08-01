#ifndef EDITOR_SEARCH_H_
#define EDITOR_SEARCH_H_

#include <string.h>
#include <stdlib.h>

#include "constants.h"
#include "editor_configs.h"
#include "editor_input.h"
#include "row.h"
#include "row_operations.h"


void edSearch();
void edSearchCallback(char *q, int k);

#endif // EDITOR_SEARCH_H_
