#include "util.h"

void local_nop() {}

// Modified from __export int create_args_and_load(char *cmdline)
int create_args(char *cmdline, int* d_argc, char*** d_argv)
{
	char *p, **argv;
	int argc;
	int i;

	if (!cmdline)
		return -1;

	for (argc = 0, p = cmdline; *p; argc++) {
		/* Find the end of this arg */
		while(*p && !isspace(*p))
			p++;

		/*
		 * Now skip all whitespace between arguments.
		 */
		while (*p && isspace(*p))
			p++;
	}

	/*
	 * Generate a copy of argv on the stack as this is
	 * traditionally where process arguments go.
	 *
	 * argv[0] must be the command name. Remember to allocate
	 * space for the sentinel NULL.
	 */
	argv = malloc((argc + 1) * sizeof(char *));

	for (i = 0, p = cmdline; i < argc; i++) {
		char *start;
		int len = 0;

		start = p;

		/* Find the end of this arg */
		while(*p && !isspace(*p)) {
			p++;
			len++;
		}

		argv[i] = malloc(len + 1);
		strncpy(argv[i], start, len);
		argv[i][len] = '\0';

		/*
		 * Now skip all whitespace between arguments.
		 */
		while (*p && isspace(*p))
			p++;

	}

	/* NUL-terminate */
	argv[argc] = NULL;

	*d_argc = argc;
	*d_argv = argv;

	return argc;
}