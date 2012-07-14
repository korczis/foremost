

#include "main.h"

void fatal_error (f_state * s, char *msg)
	{
	fprintf(stderr, "%s: %s%s", __progname, msg, NEWLINE);
	if (get_audit_file_open(s))
		{
		audit_msg(s, msg);
		close_audit_file(s);
		}
	exit(EXIT_FAILURE);
	}

void print_error(f_state *s, char *fn, char *msg)
{
	if (!(get_mode(s, mode_quiet)))
		fprintf(stderr, "%s: %s: %s%s", __progname, fn, msg, NEWLINE);
}

void print_message(f_state *s, char *format, va_list argp)
{
	vfprintf(stdout, format, argp);
	fprintf(stdout, "%s", NEWLINE);
}
