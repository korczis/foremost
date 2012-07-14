


/* FOREMOST
 *
 * By Jesse Kornblum and Kris Kendall
 * 
 * This is a work of the US Government. In accordance with 17 USC 105,
 * copyright protection is not available for any work of the US Government.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 */
#include "main.h"

#ifdef __WIN32

/* Allows us to open standard input in binary mode by default 
   See http://gnuwin32.sourceforge.net/compile.html for more */
int _CRT_fmode = _O_BINARY;
#endif

void catch_alarm(int signum)
{
	signal_caught = signum;
	signal(signum, catch_alarm);
}

void register_signal_handler(void)
{
	signal_caught = 0;

	if (signal(SIGINT, catch_alarm) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, catch_alarm) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);

#ifndef __WIN32

	/* Note: I haven't found a way to get notified of
     console resize events in Win32.  Right now the statusbar
     will be too long or too short if the user decides to resize
     their console window while foremost runs.. */

	/* RBF - Handle TTY events  */

	// The function setttywidth is in the old helpers.c
	// signal(SIGWINCH, setttywidth);
#endif
}

void try_msg(void)
{
	fprintf(stderr, "Try `%s -h` for more information.%s", __progname, NEWLINE);
}

/* The usage function should, at most, display 22 lines of text to fit
   on a single screen */
void usage(void)
{
	fprintf(stderr, "%s version %s by %s.%s", __progname, VERSION, AUTHOR, NEWLINE);
	fprintf(stderr,
			"%s %s [-v|-V|-h|-T|-Q|-q|-a|-w-d] [-t <type>] [-s <blocks>] [-k <size>] \n\t[-b <size>] [-c <file>] [-o <dir>] [-i <file] %s%s",
		CMD_PROMPT,
			__progname,
			NEWLINE,
			NEWLINE);
	fprintf(stderr, "-V  - display copyright information and exit%s", NEWLINE);
	fprintf(stderr, "-t  - specify file type.  (-t jpeg,pdf ...) %s", NEWLINE);
	fprintf(stderr, "-d  - turn on indirect block detection (for UNIX file-systems) %s", NEWLINE);
	fprintf(stderr, "-i  - specify input file (default is stdin) %s", NEWLINE);
	fprintf(stderr,
			"-a  - Write all headers, perform no error detection (corrupted files) %s",
			NEWLINE);
	fprintf(stderr,
			"-w  - Only write the audit file, do not write any detected files to the disk %s",
			NEWLINE);
	fprintf(stderr,
			"-o  - set output directory (defaults to %s)%s",
			DEFAULT_OUTPUT_DIRECTORY,
			NEWLINE);
	fprintf(stderr,
			"-c  - set configuration file to use (defaults to %s)%s",
			DEFAULT_CONFIG_FILE,
			NEWLINE);
	fprintf(stderr,
			"-q  - enables quick mode. Search are performed on 512 byte boundaries.%s",
			NEWLINE);
	fprintf(stderr, "-Q  - enables quiet mode. Suppress output messages. %s", NEWLINE);

	/* RBF - What should verbose mode be? */
	fprintf(stderr, "-v  - verbose mode. Logs all messages to screen%s", NEWLINE);
}

void process_command_line(int argc, char **argv, f_state *s)
{

	int		i;
	char	*ptr1, *ptr2;

	while ((i = getopt(argc, argv, "o:b:c:t:s:i:k:hqmQTadvVw")) != -1)
		{
		switch (i)
			{

			case 'v':
				set_mode(s, mode_verbose);
				break;

			case 'd':
				set_mode(s, mode_ind_blk);
				break;

			case 'w':
				set_mode(s, mode_write_audit);	/*Only write audit*/
				break;

			case 'a':
				set_mode(s, mode_write_all);	/*Write all headers*/
				break;

			case 'b':
				set_block(s, atoi(optarg));
				break;

			case 'o':
				set_output_directory(s, optarg);
				break;

			case 'q':
				set_mode(s, mode_quick);
				break;

			case 'Q':
				set_mode(s, mode_quiet);
				break;

			case 'c':
				set_config_file(s, optarg);
				break;

			case 'm':
				set_mode(s, mode_multi_file);

			case 'k':
				set_chunk(s, atoi(optarg));
				break;

			case 's':
				set_skip(s, atoi(optarg));
				break;

			case 'i':
				set_input_file(s, optarg);
				break;

			case 'T':
				s->time_stamp = TRUE;
				break;

			case 't':

				/*See if we have multiple file types to define*/
				ptr1 = ptr2 = optarg;
				while (1)
					{
					if (!*ptr2)
						{
						if (!set_search_def(s, ptr1, 0))
							{
							usage();
							exit(EXIT_SUCCESS);
							}
						break;
						}

					if (*ptr2 == ',')
						{
						*ptr2 = '\0';
						if (!set_search_def(s, ptr1, 0))
							{
							usage();
							exit(EXIT_SUCCESS);
							}

						*ptr2++ = ',';
						ptr1 = ptr2;
						}
					else
						{
						ptr2++;
						}
					}
				break;

			case 'h':
				usage();
				exit(EXIT_SUCCESS);

			case 'V':
				printf("%s%s", VERSION, NEWLINE);

				/* We could just say printf(COPYRIGHT), but that's a good way
	 to introduce a format string vulnerability. Better to always
	 use good programming practice... */
				printf("%s", COPYRIGHT);
				exit(EXIT_SUCCESS);

			default:
				try_msg();
				exit(EXIT_FAILURE);

			}

		}

#ifdef __DEBUG
	dump_state(s);
#endif

}

int main(int argc, char **argv)
{

	FILE	*testFile = NULL;
	f_state *s = (f_state *)malloc(sizeof(f_state));
	int		input_files = 0;
	char	**temp = argv;
	DIR* 	dir;

#ifndef __GLIBC__
	__progname = basename(argv[0]);
#endif

	/*Initialize the global state struct*/
	if (initialize_state(s, argc, argv))
		fatal_error(s, "Unable to initialize state");

	register_signal_handler();
	process_command_line(argc, argv, s);

	load_config_file(s);

	if (s->num_builtin == 0)
		{

		/*Nothing specified via the command line or the conf
	file so default to all builtin search types*/
		set_search_def(s, "all", 0);
		}
	
	if (create_output_directory(s))
		fatal_error(s, "Unable to open output directory");	

	if (!get_mode(s, mode_write_audit))
		{
		create_sub_dirs(s);
		}

	if (open_audit_file(s))
		fatal_error(s, "Can't open audit file");

	/* Scan for valid files to open */
	while (*argv != NULL)
	{
		if(strcmp(*argv,"-c")==0)
		{
			/*jump past the conf file so we don't process it.*/
			argv+=2;
		}
		testFile = fopen(*argv, "rb");
		if (testFile)
		{
			fclose(testFile);
			dir = opendir(*argv);
			
			if(!strstr(s->config_file,*argv)!=0 && !dir)
			{
				input_files++;
			}
			
			if(dir) closedir(dir);		
		}

		++argv;
	}

	argv = temp;
	if (input_files > 1)
		{
		set_mode(s, mode_multi_file);
		}

	++argv;
	while (*argv != NULL)
		{
		testFile = fopen(*argv, "rb");

		if (testFile)
			{
				fclose(testFile);
				dir = opendir(*argv);
				if(!strstr(s->config_file,*argv)!=0 && !dir)
				{
					set_input_file(s, *argv);
					process_file(s);
				}
				if(dir) closedir(dir);	
			}

		++argv;
		}

	if (input_files == 0)
		{

		//printf("using stdin\n");
		process_stdin(s);
		}

	print_stats(s);

	/*Lets try to clean up some of the extra sub_dirs*/
	cleanup_output(s);

	if (close_audit_file(s))
		{

		/* Hells bells. This is bad, but really, what can we do about it? 
       Let's just report the error and try to get out of here! */
		print_error(s, AUDIT_FILE_NAME, "Error closing audit file");
		}

	free_state(s);
	free(s);
	return EXIT_SUCCESS;
}
