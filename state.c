

#include "main.h"

int initialize_state (f_state * s, int argc, char **argv)
	{
	char	**argv_copy = argv;

	/* The routines in current_time return statically allocated memory.
     We strdup the result so that we don't accidently free() the wrong
     thing later on. */
	s->start_time = strdup(current_time());
	wildcard = '?';
	s->audit_file_open = FALSE;
	s->mode = DEFAULT_MODE;
	s->input_file = NULL;
	s->fileswritten = 0;
	s->block_size = 512;

	/* We use the setter fuctions here to call realpath */
	set_config_file(s, DEFAULT_CONFIG_FILE);
	set_output_directory(s, DEFAULT_OUTPUT_DIRECTORY);

	s->invocation = (char *)malloc(sizeof(char) * MAX_STRING_LENGTH);
	s->invocation[0] = 0;
	s->chunk_size = CHUNK_SIZE;
	s->num_builtin = 0;
	s->skip = 0;
	s->time_stamp = FALSE;
	do
		{
		strncat(s->invocation, *argv_copy, MAX_STRING_LENGTH - strlen(s->invocation));
		strncat(s->invocation, " ", MAX_STRING_LENGTH - strlen(s->invocation));
		++argv_copy;
		}
	while (*argv_copy);

	return FALSE;
	}

void free_state(f_state *s)
{
	free(s->start_time);
	free(s->output_directory);
	free(s->config_file);
}

int get_audit_file_open(f_state *s)
{
	return (s->audit_file_open);
}

char *get_invocation(f_state *s)
{
	return (s->invocation);
}

char *get_start_time(f_state *s)
{
	return (s->start_time);
}

char *get_config_file(f_state *s)
{
	return (s->config_file);
}

int set_config_file(f_state *s, char *fn)
{
	char	temp[PATH_MAX];

	/* If the configuration file doesn't exist, this realpath will return
     NULL. We don't error check here as the user may specify a file
     that doesn't currently exist */
	realpath(fn, temp);

	/* RBF - Does this create a memory leak? What happens to the old value? */
	s->config_file = strdup(temp);
	return FALSE;
}

char *get_output_directory(f_state *s)
{
	return (s->output_directory);
}

int set_output_directory(f_state *s, char *fn)
{
	char	temp[PATH_MAX];
  int 	fullpathlen=0;
	/* We don't error check here as it's quite possible that the
     output directory doesn't exist yet. If it doesn't, realpath
     resolves the path correctly, but still returns NULL. */
  //strncpy(s->output_directory,fn,PATH_MAX);
  
	realpath(fn, temp);
	fullpathlen=strlen(temp);

	if(fullpathlen!=0)
	{
		s->output_directory = strdup(temp);
	}
	else
	{
		/*Realpath failed just use cwd*/
		s->output_directory = strdup(fn);
	}
	return FALSE;
}

int get_mode(f_state *s, off_t check_mode)
{
	return (s->mode & check_mode);
}

void set_mode(f_state *s, off_t new_mode)
{
	s->mode |= new_mode;
}

void set_chunk(f_state *s, int size)
{
	s->chunk_size = size;
}

void set_skip(f_state *s, int size)
{
	s->skip = size;
}

void set_block(f_state *s, int size)
{
	s->block_size = size;
}

void write_audit_header(f_state *s)
{
	audit_msg(s, "Foremost version %s by %s", VERSION, AUTHOR);
	audit_msg(s, "Audit File");
	audit_msg(s, "");
	audit_msg(s, "Foremost started at %s", get_start_time(s));
	audit_msg(s, "Invocation: %s", get_invocation(s));
	audit_msg(s, "Output directory: %s", get_output_directory(s));
	audit_msg(s, "Configuration file: %s", get_config_file(s));
}

int open_audit_file(f_state *s)
{
	char	fn[MAX_STRING_LENGTH];

	snprintf(fn,
			 MAX_STRING_LENGTH,
			 "%s%c%s",
			 get_output_directory(s),
			 DIR_SEPARATOR,
			 AUDIT_FILE_NAME);

	if ((s->audit_file = fopen(fn, "w")) == NULL)
		{
		print_error(s, fn, strerror(errno));
		fatal_error(s, "Can't open audit file");
		}

	s->audit_file_open = TRUE;
	write_audit_header(s);

	return FALSE;
}

int close_audit_file(f_state *s)
{
	audit_msg(s, FOREMOST_DIVIDER);
	audit_msg(s, "");
	audit_msg(s, "Foremost finished at %s", current_time());

	if (fclose(s->audit_file))
		{
		print_error(s, AUDIT_FILE_NAME, strerror(errno));
		return TRUE;
		}

	return FALSE;
}

void audit_msg(f_state *s, char *format, ...)
{
	va_list argp;
	va_start(argp, format);

	if (get_mode(s, mode_verbose)) {
		print_message(s, format, argp);
		va_end(argp);
		va_start(argp, format);
	}

	vfprintf(s->audit_file, format, argp);
	va_end(argp);

	fprintf(s->audit_file, "%s", NEWLINE);
	fflush(stdout);
}

void set_input_file(f_state *s, char *filename)
{
	s->input_file = (char *)malloc((strlen(filename) + 1) * sizeof(char));
	strncpy(s->input_file, filename, strlen(filename) + 1);
}

/*Initialize any search specs*/
int init_builtin(f_state *s, int type, char *suffix, char *header, char *footer, int header_len,
				 int footer_len, u_int64_t max_len, int case_sen)
{

	int i = s->num_builtin;

	search_spec[i].type = type;
	search_spec[i].suffix = (char *)malloc((strlen(suffix)+1) * sizeof(char));
	search_spec[i].num_markers = 0;
	strcpy(search_spec[i].suffix, suffix);

	search_spec[i].header_len = header_len;
	search_spec[i].footer_len = footer_len;

	search_spec[i].max_len = max_len;
	search_spec[i].found = 0;
	search_spec[i].header = (unsigned char *)malloc(search_spec[i].header_len * sizeof(unsigned char));
	search_spec[i].footer = (unsigned char *)malloc(search_spec[i].footer_len * sizeof(unsigned char));
	search_spec[i].case_sen = case_sen;
	memset(search_spec[i].comment, 0, COMMENT_LENGTH - 1);

	memcpy(search_spec[i].header, header, search_spec[i].header_len);
	memcpy(search_spec[i].footer, footer, search_spec[i].footer_len);

	init_bm_table(search_spec[i].header,
				  search_spec[i].header_bm_table,
				  search_spec[i].header_len,
				  search_spec[i].case_sen,
				  SEARCHTYPE_FORWARD);
	init_bm_table(search_spec[i].footer,
				  search_spec[i].footer_bm_table,
				  search_spec[i].footer_len,
				  search_spec[i].case_sen,
				  SEARCHTYPE_FORWARD);
	s->num_builtin++;

	return i;
}

/*Markers are a method to search for any unique information besides just the header and the footer*/
void add_marker(f_state *s, int index, char *marker, int markerlength)
{
	int i = search_spec[index].num_markers;
	if (marker == NULL)
		{
		search_spec[index].num_markers = 0;
		return;
		}

	search_spec[index].markerlist[i].len = markerlength;
	search_spec[index].markerlist[i].value = (unsigned char *)malloc(search_spec[index].markerlist[i].len * sizeof(unsigned char));

	memcpy(search_spec[index].markerlist[i].value, marker, search_spec[index].markerlist[i].len);
	init_bm_table(search_spec[index].markerlist[i].value,
				  search_spec[index].markerlist[i].marker_bm_table,
				  search_spec[index].markerlist[i].len,
				  TRUE,
				  SEARCHTYPE_FORWARD);
	search_spec[index].num_markers++;
}

/*Initial every search spec we know about*/
void init_all(f_state *state)
{
	int index = 0;
	init_builtin(state, JPEG, "jpg", "\xff\xd8\xff", "\xff\xd9", 3, 2, 20 * MEGABYTE, TRUE);
	index = init_builtin(state, GIF, "gif", "\x47\x49\x46\x38", "\x00\x3b", 4, 2, MEGABYTE, TRUE);
	add_marker(state, index, "\x00\x00\x3b", 3);
	init_builtin(state, BMP, "bmp", "BM", NULL, 2, 0, 2 * MEGABYTE, TRUE);
	init_builtin(state,
				 WMV,
				 "wmv",
				 "\x30\x26\xB2\x75\x8E\x66\xCF\x11",
				 "\xA1\xDC\xAB\x8C\x47\xA9",
				 8,
				 6,
				 40 * MEGABYTE,
				 TRUE);
	init_builtin(state, MOV, "mov", "moov", NULL, 4, 0, 40 * MEGABYTE, TRUE);
	init_builtin(state, MP4, "mp4", "\x00\x00\x00\x1c\x66\x74\x79\x70", NULL, 8, 0, 600 * MEGABYTE, TRUE);
	init_builtin(state, RIFF, "rif", "RIFF", "INFO", 4, 4, 20 * MEGABYTE, TRUE);
	init_builtin(state, HTM, "htm", "<html", "</html>", 5, 7, MEGABYTE, FALSE);
	init_builtin(state,
				 OLE,
				 "ole",
				 "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\x00\x00\x00\x00\x00\x00\x00\x00",
				 NULL,
				 16,
				 0,
				 5 * MEGABYTE,
				 TRUE);
	init_builtin(state,
				 ZIP,
				 "zip",
				 "\x50\x4B\x03\x04",
				 "\x4b\x05\x06\x00",
				 4,
				 4,
				 100 * MEGABYTE,
				 TRUE);
	init_builtin(state,
				 RAR,
				 "rar",
				 "\x52\x61\x72\x21\x1A\x07\x00",
				 "\x00\x00\x00\x00\x00\x00\x00\x00",
				 7,
				 8,
				 100 * MEGABYTE,
				 TRUE);
	init_builtin(state, EXE, "exe", "MZ", NULL, 2, 0, 1 * MEGABYTE, TRUE);

	index = init_builtin(state,
						 PNG,
						 "png",
						 "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A",
						 "IEND",
						 8,
						 4,
						 1 * MEGABYTE,
						 TRUE);
	index = init_builtin(state,
						 MPG,
						 "mpg",
						 "\x00\x00\x01\xba",
						 "\x00\x00\x01\xb9",
						 4,
						 4,
						 50 * MEGABYTE,
						 TRUE);
	add_marker(state, index, "\x00\x00\x01", 3);

	index = init_builtin(state, PDF, "pdf", "%PDF-1.", "%%EOF", 7, 5, 40 * MEGABYTE, TRUE);
	add_marker(state, index, "/L ", 3);
	add_marker(state, index, "obj", 3);
	add_marker(state, index, "/Linearized", 11);
	add_marker(state, index, "/Length", 7);
}

/*Process any command line args following the -t switch)*/
int set_search_def(f_state *s, char *ft, u_int64_t max_file_size)
{
	int index = 0;

	if (strcmp(ft, "jpg") == 0 || strcmp(ft, "jpeg") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;
		init_builtin(s, JPEG, "jpg", "\xff\xd8\xff", "\xff\xd9", 3, 2, max_file_size, TRUE);
		}
	else if (strcmp(ft, "gif") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;
		index = init_builtin(s,
							 GIF,
							 "gif",
							 "\x47\x49\x46\x38",
							 "\x00\x3b",
							 4,
							 2,
							 max_file_size,
							 TRUE);

		add_marker(s, index, "\x00\x00\x3b", 3);
		}
	else if (strcmp(ft, "bmp") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 2 * MEGABYTE;

		init_builtin(s, BMP, "bmp", "BM", NULL, 2, 0, max_file_size, TRUE);
		}
	else if (strcmp(ft, "mp4") == 0)
		{
			init_builtin(s, MP4, "mp4", "\x00\x00\x00\x1c\x66\x74\x79\x70", NULL, 8, 0, 600 * MEGABYTE, TRUE);
		}
	else if (strcmp(ft, "exe") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;

		init_builtin(s, EXE, "exe", "MZ", NULL, 2, 0, max_file_size, TRUE);
		}
	else if (strcmp(ft, "elf") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;

		init_builtin(s, ELF, "elf", "0x7fELF", NULL, 4, 0, max_file_size, TRUE);
		}	
	else if (strcmp(ft, "reg") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 2 * MEGABYTE;

		init_builtin(s, REG, "reg", "regf", NULL, 4, 0, max_file_size, TRUE);

		}	
	else if (strcmp(ft, "mpg") == 0 || strcmp(ft, "mpeg") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 50 * MEGABYTE;

		//20000000 \x00\x00\x01\xb3      \x00\x00\x01\xb7 //system data
		index = init_builtin(s,
							 MPG,
							 "mpg",
							 "\x00\x00\x01\xba",
							 "\x00\x00\x01\xb9",
							 4,
							 4,
							 max_file_size,
							 TRUE);
		add_marker(s, index, "\x00\x00\x01", 3);

		/*
	    add_marker(s,index,"\x00\x00\x01\xBB",4);
	    add_marker(s,index,"\x00\x00\x01\xBE",4);
	    add_marker(s,index,"\x00\x00\x01\xB3",4);
	    */
		}
	else if (strcmp(ft, "wmv") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;

		init_builtin(s,
					 WMV,
					 "wmv",
					 "\x30\x26\xB2\x75\x8E\x66\xCF\x11",
					 "\xA1\xDC\xAB\x8C\x47\xA9",
					 8,
					 6,
					 max_file_size,
					 TRUE);
		}
	else if (strcmp(ft, "avi") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;

		init_builtin(s, AVI, "avi", "RIFF", "INFO", 4, 4, max_file_size, TRUE);
		}

	else if (strcmp(ft, "rif") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;
		init_builtin(s, RIFF, "rif", "RIFF", "INFO", 4, 4, max_file_size, TRUE);
		}
	else if (strcmp(ft, "wav") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;
		init_builtin(s, WAV, "wav", "RIFF", "INFO", 4, 4, max_file_size, TRUE);

		}
	else if (strcmp(ft, "html") == 0 || strcmp(ft, "htm") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;
		init_builtin(s, HTM, "htm", "<html", "</html>", 5, 7, max_file_size, FALSE);
		}

	else if (strcmp(ft, "ole") == 0 || strcmp(ft, "office") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;
		init_builtin(s,
					 OLE,
					 "ole",
					 "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\x00\x00\x00\x00\x00\x00\x00\x00",
					 NULL,
					 16,
					 0,
					 max_file_size,
					 TRUE);
		}
	else if (strcmp(ft, "doc") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;
		init_builtin(s,
					 DOC,
					 "doc",
					 "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\x00\x00\x00\x00\x00\x00\x00\x00",
					 NULL,
					 16,
					 0,
					 max_file_size,
					 TRUE);
		}
	else if (strcmp(ft, "xls") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 XLS,
					 "xls",
					 "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\x00\x00\x00\x00\x00\x00\x00\x00",
					 NULL,
					 16,
					 0,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "ppt") == 0)
		{

		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;
		init_builtin(s,
					 PPT,
					 "ppt",
					 "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\x00\x00\x00\x00\x00\x00\x00\x00",
					 NULL,
					 16,
					 0,
					 max_file_size,
					 TRUE);
		}
	else if (strcmp(ft, "zip") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 100 * MEGABYTE;

		init_builtin(s,
					 ZIP,
					 "zip",
					 "\x50\x4B\x03\x04",
					 "\x50\x4b\x05\x06",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "rar") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 100 * MEGABYTE;

		init_builtin(s,
					 RAR,
					 "rar",
					 "\x52\x61\x72\x21\x1A\x07\x00",
					 "\x00\x00\x00\x00\x00\x00\x00\x00",
					 7,
					 8,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "sxw") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 SXW,
					 "sxw",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "sxc") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 SXC,
					 "sxc",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "sxi") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 SXI,
					 "sxi",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "docx") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 DOCX,
					 "docx",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "pptx") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 PPTX,
					 "pptx",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "xlsx") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 10 * MEGABYTE;

		init_builtin(s,
					 XLSX,
					 "xlsx",
					 "\x50\x4B\x03\x04",
					 "\x4b\x05\x06\x00",
					 4,
					 4,
					 max_file_size,
					 TRUE);

		}
	else if (strcmp(ft, "gzip") == 0 || strcmp(ft, "gz") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 100 * MEGABYTE;

		init_builtin(s, GZIP, "gz", "\x1F\x8B", "\x00\x00\x00\x00", 2, 4, max_file_size, TRUE);
		}
	else if (strcmp(ft, "pdf") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 20 * MEGABYTE;

		index = init_builtin(s, PDF, "pdf", "%PDF-1.", "%%EOF", 7, 5, max_file_size, TRUE);
		add_marker(s, index, "/L ", 3);
		add_marker(s, index, "obj", 3);
		add_marker(s, index, "/Linearized", 11);
		add_marker(s, index, "/Length", 7);
		}
	else if (strcmp(ft, "vjpeg") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 40 * MEGABYTE;
		init_builtin(s, VJPEG, "mov", "pnot", NULL, 4, 0, max_file_size, TRUE);
		}
	else if (strcmp(ft, "mov") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 40 * MEGABYTE;

		init_builtin(s, MOV, "mov", "moov", NULL, 4, 0, max_file_size, TRUE);
		}
	else if (strcmp(ft, "wpd") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;

		init_builtin(s, WPD, "wpd", "\xff\x57\x50\x43", NULL, 4, 0, max_file_size, TRUE);
		}
	else if (strcmp(ft, "cpp") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;

		index = init_builtin(s, CPP, "cpp", "#include", "char", 8, 4, max_file_size, TRUE);
		add_marker(s, index, "int", 3);
		}
	else if (strcmp(ft, "png") == 0)
		{
		if (max_file_size == 0)
			max_file_size = 1 * MEGABYTE;
		index = init_builtin(s,
							 PNG,
							 "png",
							 "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A",
							 "IEND",
							 8,
							 4,
							 max_file_size,
							 TRUE);
		}
	else if (strcmp(ft, "all") == 0)
		{
		init_all(s);
		}
	else
		{
		return FALSE;
		}

	return TRUE;

}

void init_bm_table(unsigned char *needle, size_t table[UCHAR_MAX + 1], size_t len, int casesensitive,
				   int searchtype)
{
	size_t	i = 0, j = 0, currentindex = 0;

	for (i = 0; i <= UCHAR_MAX; i++)
		table[i] = len;
	for (i = 0; i < len; i++)
		{
		if (searchtype == SEARCHTYPE_REVERSE)
			{

			currentindex = i;			//If we are running our searches backwards
			//we count from the beginning of the string
			}
		else
			{
			currentindex = len - i - 1; //Count from the back of string
			}

		if (needle[i] == wildcard)		//No skip entry can advance us past the last wildcard in the string
			{
			for (j = 0; j <= UCHAR_MAX; j++)
				table[j] = currentindex;
			}

		table[(unsigned char)needle[i]] = currentindex;
		if (!casesensitive)
			{

			//RBF - this is a little kludgy but it works and this isn't the part
			//of the code we really need to worry about optimizing...
			//If we aren't case sensitive we just set both the upper and lower case
			//entries in the jump table.
			table[tolower(needle[i])] = currentindex;
			table[toupper(needle[i])] = currentindex;
			}
		}
}

#ifdef __DEBUG
void dump_state(f_state *s)
{
	printf("Current state:\n");
	printf("Config file: %s\n", s->config_file);
	printf("Output directory: %s\n", s->output_directory);
	printf("Mode: %llu\n", s->mode);

}
#endif
