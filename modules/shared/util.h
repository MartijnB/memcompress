#ifndef _MOD_UTIL_H
#define _MOD_UTIL_H

#include <com32.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int create_args(char *cmdline, int* d_argc, char*** d_argv);

#endif