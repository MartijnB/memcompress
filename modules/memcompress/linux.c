#include "memcompress.h"

// This is a nasty workaround for a limitation of Syslinux.
// If you start another module and this module has no dependency
// to the original module, the original module is uploaded. However,
// the memcompress module adds a callback hook in the (not exported)
// memory map handler list.

// At the same time, it is unwanted to copy all functionality of
// linux loading module into this module. So we "include" some
// parts of the original code combined with some preprocessor
// magic to prevent conflicting symbols.

#include "../lib/syslinux/load_linux.c"

#define syslinux_boot_linux bios_boot_linux
#define find_argument linux_find_argument
#define main linuxc32_main
#include "../modules/linux.c"
#undef main

char g_cmdline_buffer[KERNEL_CMD_LINE_SIZE];
char* g_cmdline_buffer_p = g_cmdline_buffer;

int create_args(char *cmdline, int* d_argc, char*** d_argv);

const char* linux_get_cmdline(void)
{
	return g_cmdline_buffer;
}

void linux_append_cmdline(const char *format, ...)
{
	va_list args;
    va_start(args, format);

    g_cmdline_buffer_p += vsnprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), format, args);

    va_end(args);
}

void boot_linux(void)
{
	int   linux_argc;
    char** linux_argv;

    create_args(g_cmdline_buffer, &linux_argc, &linux_argv);

    printf("Boot kernel...\n");

    linuxc32_main(linux_argc, linux_argv);
}

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