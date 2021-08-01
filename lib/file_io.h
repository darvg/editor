#ifndef FILE_IO_H_
#define FILE_IO_H_

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "editor_configs.h"
#include "editor_input.h"
#include "row.h"
#include "terminal_config.h"


void edOpen(char *fname);
void *edRowsToString(int *bufLen);
void edSave();

#endif // FILE_IO_H_
