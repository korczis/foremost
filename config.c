

#include "main.h"

int translate (char *str)
	{
	char	next;
	char	*rd = str, *wr = str, *bad;
	char	temp[1 + 3 + 1];
	char	ch;

	if (!*rd)					//If it's a null string just return
		{
		return 0;
		}

	while (*rd)
		{

		/* Is it an escaped character ? */
		if (*rd == '\\')
			{
			rd++;
			switch (*rd)
				{
				case '\\':
					*rd++;
					*wr++ = '\\';
					break;

				case 'a':
					*rd++;
					*wr++ = '\a';
					break;

				case 's':
					*rd++;
					*wr++ = ' ';
					break;

				case 'n':
					*rd++;
					*wr++ = '\n';
					break;

				case 'r':
					*rd++;
					*wr++ = '\r';
					break;

				case 't':
					*rd++;
					*wr++ = '\t';
					break;

				case 'v':
					*rd++;
					*wr++ = '\v';
					break;

				/* Hexadecimal/Octal values are treated in one place using strtoul() */
				case 'x':
				case '0':
				case '1':
				case '2':
				case '3':
					next = *(rd + 1);
					if (next < 48 || (57 < next && next < 65) || (70 < next && next < 97) || next > 102)
						break;	//break if not a digit or a-f, A-F
					next = *(rd + 2);
					if (next < 48 || (57 < next && next < 65) || (70 < next && next < 97) || next > 102)
						break;	//break if not a digit or a-f, A-F
					temp[0] = '0';
					bad = temp;
					strncpy(temp + 1, rd, 3);
					temp[4] = '\0';
					ch = strtoul(temp, &bad, 0);
					if (*bad == '\0')
						{
						*wr++ = ch;
						rd += 3;
						}		/* else INVALID CHARACTER IN INPUT ('\\' followed by *rd) */
					break;

				default:		/* INVALID CHARACTER IN INPUT (*rd)*/
					*wr++ = '\\';
					break;
				}
			}

		/* Unescaped characters go directly to the output */
		else
			*wr++ = *rd++;
		}
	*wr = '\0';					//Null terminate the string that we just created...
	return wr - str;
	}

char *skipWhiteSpace(char *str)
{
	while (isspace(str[0]))
		str++;
	return str;
}

int extractSearchSpecData(f_state *state, char **tokenarray)
{

	/* Process a normal line with 3-4 tokens on it
   token[0] = suffix
   token[1] = case sensitive
   token[2] = size to snarf
   token[3] = begintag
   token[4] = endtag (optional)
   token[5] = search for footer from back of buffer flag and other options (whew!)
*/

	/* Allocate the memory for these lines.... */
	s_spec	*s = &search_spec[state->num_builtin];

	s->suffix = malloc(MAX_SUFFIX_LENGTH * sizeof(char));
	s->header = malloc(MAX_STRING_LENGTH * sizeof(char));
	s->footer = malloc(MAX_STRING_LENGTH * sizeof(char));
	s->type = CONF;
	if (!strncasecmp(tokenarray[0], FOREMOST_NOEXTENSION_SUFFIX, strlen(FOREMOST_NOEXTENSION_SUFFIX)
		))
		{
		s->suffix[0] = ' ';
		s->suffix[1] = 0;
		}
	else
		{

		/* Assign the current line to the SearchSpec object */
		memcpy(s->suffix, tokenarray[0], MAX_SUFFIX_LENGTH);
		}

	/* Check for case sensitivity */
	s->case_sen = (!strncasecmp(tokenarray[1], "y", 1) || !strncasecmp(tokenarray[1], "yes", 3));

	s->max_len = atoi(tokenarray[2]);

	/* Determine which search type we want to use for this needle */
	s->searchtype = SEARCHTYPE_FORWARD;
	if (!strncasecmp(tokenarray[5], "REVERSE", strlen("REVERSE")))
		{

		s->searchtype = SEARCHTYPE_REVERSE;
		}
	else if (!strncasecmp(tokenarray[5], "NEXT", strlen("NEXT")))
		{
		s->searchtype = SEARCHTYPE_FORWARD_NEXT;
		}

	// this is the default, but just if someone wants to provide this value just to be sure
	else if (!strncasecmp(tokenarray[5], "FORWARD", strlen("FORWARD")))
		{
		s->searchtype = SEARCHTYPE_FORWARD;
		}
	else if (!strncasecmp(tokenarray[5], "ASCII", strlen("ASCII")))
		{
			//fprintf(stderr,"Setting ASCII TYPE\n");
		s->searchtype = SEARCHTYPE_ASCII;
		}

	/* Done determining searchtype */

	/* We copy the tokens and translate them from the file format.
   The translate() function does the translation and returns
   the length of the argument being translated */
	s->header_len = translate(tokenarray[3]);
	memcpy(s->header, tokenarray[3], s->header_len);
	s->footer_len = translate(tokenarray[4]);
	memcpy(s->footer, tokenarray[4], s->footer_len);

	init_bm_table(s->header, s->header_bm_table, s->header_len, s->case_sen, s->searchtype);
	init_bm_table(s->footer, s->footer_bm_table, s->footer_len, s->case_sen, s->searchtype);

	return TRUE;
}

int process_line(f_state *s, char *buffer, int line_number)
{

	char	*buf = buffer;
	char	*token;
	char	**tokenarray = (char **)malloc(6 * sizeof(char[MAX_STRING_LENGTH]));
	int		i = 0, len = strlen(buffer);

	/* Any line that ends with a CTRL-M (0x0d) has been processed
   by a DOS editor. We will chop the CTRL-M to ignore it */
	if (buffer[len - 2] == 0x0d && buffer[len - 1] == 0x0a)
		{
		buffer[len - 2] = buffer[len - 1];
		buffer[len - 1] = buffer[len];
		}

	buf = (char *)skipWhiteSpace(buf);
	token = strtok(buf, " \t\n");

	/* Any line that starts with a '#' is a comment and can be skipped */
	if (token == NULL || token[0] == '#')
		{
		return TRUE;
		}

	/* Check for the wildcard */
	if (!strncasecmp(token, "wildcard", 9))
		{
		if ((token = strtok(NULL, " \t\n")) != NULL)
			{
			translate(token);
			}
		else
			{
			return TRUE;
			}

		if (strlen(token) > 1)
			{
			fprintf(stderr,
					"Warning: Wildcard can only be one character,"
					" but you specified %zu characters.\n"
				"         Using the first character, \"%c\", as the wildcard.\n",
			strlen(token),
					token[0]);
			}

		wildcard = token[0];
		return TRUE;
		}

	while (token && (i < NUM_SEARCH_SPEC_ELEMENTS))
		{
		tokenarray[i] = token;
		i++;
		token = strtok(NULL, " \t\n");
		}

	switch (NUM_SEARCH_SPEC_ELEMENTS - i)
		{
		case 2:
			tokenarray[NUM_SEARCH_SPEC_ELEMENTS - 1] = "";
			tokenarray[NUM_SEARCH_SPEC_ELEMENTS - 2] = "";
			break;

		case 1:
			tokenarray[NUM_SEARCH_SPEC_ELEMENTS - 1] = "";
			break;

		case 0:
			break;

		default:
			fprintf(stderr, "\nERROR: In line %d of the configuration file.\n", line_number);
			return FALSE;
			return TRUE;

		}

	if (!extractSearchSpecData(s, tokenarray))
		{
		fprintf(stderr,
				"\nERROR: Unknown error on line %d of the configuration file.\n",
				line_number);
		}

	s->num_builtin++;

	return TRUE;
}

int load_config_file(f_state *s)
{
	FILE	*f;
	char	*buffer = (char *)malloc(MAX_STRING_LENGTH * sizeof(char));
	off_t	line_number = 0;

#ifdef __DEBUG
	printf("About to open config file %s%s", get_config_file(s), NEWLINE);
#endif

	if ((f = fopen(get_config_file(s), "r")) == NULL)
	{

		/*Can't find  a conf in the current directory
    * So lets try the /usr/local/etc*/
#ifdef __WIN32
		set_config_file(s, "/Program Files/foremost/foremost.conf");
#else
		set_config_file(s, "/usr/local/etc/foremost.conf");
#endif
		if ((f = fopen(get_config_file(s), "r")) == NULL)
			{
			print_error(s, get_config_file(s), strerror(errno));
			free(buffer);
			return TRUE;
			}

	}

	while (fgets(buffer, MAX_STRING_LENGTH, f))
		{
		++line_number;
		if (!process_line(s, buffer, line_number))
			{
			free(buffer);
			fclose(f);
			return TRUE;

			}
		}

	fclose(f);
	free(buffer);
	return FALSE;
}
