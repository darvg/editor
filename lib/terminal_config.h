#ifndef TERMINAL_CONFIG_H_
#define TERMINAL_CONFIG_H_

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "constants.h"
#include "editor_configs.h"
#include "editor_output.h"


void error_exit(const char *s);
void enableRawMode();
void disableRawMode();
int edReadKey();
int getWindowSize(int *rows, int *cols);

#endif // TERMINAL_CONFIG_H_
