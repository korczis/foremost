

#include "main.h"

int is_empty_directory (DIR * temp)
	{

	/* Empty directories contain two entries for . and .. 
     A directory with three entries, therefore, is not empty */
	if (readdir(temp) && readdir(temp) && readdir(temp))
		return FALSE;

	return TRUE;
	}

/*Try to cleanup the ouput directory if nothing to a sub-dir*/
void cleanup_output(f_state *s)
{
	char			dir_name[MAX_STRING_LENGTH];

	DIR				*temp;
	DIR				*outputDir;
	struct dirent	*entry;

	if ((outputDir = opendir(get_output_directory(s))) == NULL)
		{

		/*Error?*/
		}

	while ((entry = readdir(outputDir)))
		{
		memset(dir_name, 0, MAX_STRING_LENGTH - 1);
		strcpy(dir_name, get_output_directory(s));
		strcat(dir_name, "/");
		strcat(dir_name, entry->d_name);
		temp = opendir(dir_name);
		if (temp != NULL)
			{
			if (is_empty_directory(temp))
				{
				rmdir(dir_name);
				}
			}

		}

}

int make_new_directory(f_state *s, char *fn)
{

#ifdef __WIN32

	#ifndef __CYGWIN
fprint(stderr,"Calling mkdir with\n");	
	if (mkdir(fn))
	#endif

#else
		mode_t	new_mode =
			(
				S_IRUSR |
				S_IWUSR |
				S_IXUSR |
				S_IRGRP |
				S_IWGRP |
				S_IXGRP |
				S_IROTH |
				S_IWOTH
			);
	if (mkdir(fn, new_mode))
#endif
		{
		if (errno != EEXIST)
			{
			print_error(s, fn, strerror(errno));
			return TRUE;
			}
		}

	return FALSE;
}

/*Clean the timestamped dir name to make it a little more file system friendly*/
char *clean_time_string(char *time)
{
	int len = strlen(time);
	int i = 0;

	for (i = 0; i < len; i++)
	{
#ifdef __WIN32
		if (time[i] == ':' && time[i + 1] != '\\')
			{
			time[i] = '_';
			}

#else
		if (time[i] == ' ' || time[i] == ':')
			{
			time[i] = '_';
			}
#endif
	}

	return time;
}

int create_output_directory(f_state *s)
{
	DIR		*d;
	char	dir_name[MAX_STRING_LENGTH];
  
	memset(dir_name, 0, MAX_STRING_LENGTH - 1);
	if (s->time_stamp)
		{
		strcpy(dir_name, get_output_directory(s));
		strcat(dir_name, "_");
		strcat(dir_name, get_start_time(s));
		clean_time_string(dir_name);
		set_output_directory(s, dir_name);
		}
#ifdef DEBUG
	printf("Checking output directory %s\n", get_output_directory(s));
#endif

	if ((d = opendir(get_output_directory(s))) != NULL)
		{

		/* The directory exists already. It MUST be empty for us to continue */
		if (!is_empty_directory(d))
			{
			printf("ERROR: %s is not empty\n \tPlease specify another directory or run with -T.\n",
				   get_output_directory(s));

			exit(EXIT_FAILURE);
			}

		/* The directory exists and is empty. We're done! */
		closedir(d);
		return FALSE;
		}

	/* The error value ENOENT means that either the directory doesn't exist,
     which is fine, or that the filename is zero-length, which is bad.
     All other errors are, of course, bad. 
*/
	if (errno != ENOENT)
		{
		print_error(s, get_output_directory(s), strerror(errno));
		return TRUE;
		}

	if (strlen(get_output_directory(s)) == 0)
		{

		/* Careful! Calling print_error will try to display a filename
       that is zero characters! In theory this should never happen 
       as our call to realpath should avoid this. But we'll play it safe. */
		print_error(s, "(output_directory)", "Output directory name unknown");
		return TRUE;
		}

	return (make_new_directory(s, get_output_directory(s)));
}

/*Create file type sub dirs, can get tricky when multiple types use one 
 extraction algorithm (OLE)*/
int create_sub_dirs(f_state *s)
{
	int		i = 0;
	int		j = 0;
	char	dir_name[MAX_STRING_LENGTH];
	char	ole_types[7][4] = { "ppt", "doc", "xls", "sdw", "mbd", "vis", "ole" };
	char	riff_types[2][4] = { "avi", "wav" };
	char	zip_types[8][5] = { "sxc", "sxw", "sxi", "sx", "jar","docx","pptx","xlsx" };

	for (i = 0; i < s->num_builtin; i++)
		{
		memset(dir_name, 0, MAX_STRING_LENGTH - 1);
		strcpy(dir_name, get_output_directory(s));
		strcat(dir_name, "/");
		strcat(dir_name, search_spec[i].suffix);
		make_new_directory(s, dir_name);

		if (search_spec[i].type == OLE)
			{
			for (j = 0; j < 7; j++)
				{
				if (strstr(ole_types[j], search_spec[i].suffix))
					continue;

				memset(dir_name, 0, MAX_STRING_LENGTH - 1);
				strcpy(dir_name, get_output_directory(s));
				strcat(dir_name, "/");
				strcat(dir_name, ole_types[j]);
				make_new_directory(s, dir_name);
				}
			}
		else if (get_mode(s, mode_write_all))
			{
			for (j = 0; j < 7; j++)
				{
				if (strstr(search_spec[i].suffix, ole_types[j]))
					{
					for (j = 0; j < 7; j++)
						{
						if (strstr(ole_types[j], search_spec[i].suffix))
							continue;

						memset(dir_name, 0, MAX_STRING_LENGTH - 1);
						strcpy(dir_name, get_output_directory(s));
						strcat(dir_name, "/");
						strcat(dir_name, ole_types[j]);
						make_new_directory(s, dir_name);
						}
					break;
					}

				}
			}

		if (search_spec[i].type == EXE)
			{
			memset(dir_name, 0, MAX_STRING_LENGTH - 1);
			strcpy(dir_name, get_output_directory(s));
			strcat(dir_name, "/");
			strcat(dir_name, "dll");
			make_new_directory(s, dir_name);
			}

		if (search_spec[i].type == RIFF)
			{
			for (j = 0; j < 2; j++)
				{
				if (strstr(ole_types[j], search_spec[i].suffix))
					continue;
				memset(dir_name, 0, MAX_STRING_LENGTH - 1);
				strcpy(dir_name, get_output_directory(s));
				strcat(dir_name, "/");
				strcat(dir_name, riff_types[j]);
				make_new_directory(s, dir_name);
				}
			}
		else if (get_mode(s, mode_write_all))
			{
			for (j = 0; j < 2; j++)
				{
				if (strstr(search_spec[i].suffix, riff_types[j]))
					{
					for (j = 0; j < 2; j++)
						{
						if (strstr(ole_types[j], search_spec[i].suffix))
							continue;

						memset(dir_name, 0, MAX_STRING_LENGTH - 1);
						strcpy(dir_name, get_output_directory(s));
						strcat(dir_name, "/");
						strcat(dir_name, riff_types[j]);
						make_new_directory(s, dir_name);
						}
					break;
					}

				}
			}

		if (search_spec[i].type == ZIP)
			{
			for (j = 0; j < 8; j++)
				{
				if (strstr(ole_types[j], search_spec[i].suffix))
					continue;

				memset(dir_name, 0, MAX_STRING_LENGTH - 1);
				strcpy(dir_name, get_output_directory(s));
				strcat(dir_name, "/");
				strcat(dir_name, zip_types[j]);
				make_new_directory(s, dir_name);
				}
			}
		else if (get_mode(s, mode_write_all))
			{
			for (j = 0; j < 8; j++)
				{
				if (strstr(search_spec[i].suffix, zip_types[j]))
					{
					for (j = 0; j < 5; j++)
						{
						if (strstr(ole_types[j], search_spec[i].suffix))
							continue;

						memset(dir_name, 0, MAX_STRING_LENGTH - 1);
						strcpy(dir_name, get_output_directory(s));
						strcat(dir_name, "/");
						strcat(dir_name, zip_types[j]);
						make_new_directory(s, dir_name);
						}
					break;
					}
				}
			}

		}

	return TRUE;
}

/*We have found a file so write to disk*/
int write_to_disk(f_state *s, s_spec *needle, u_int64_t len, unsigned char *buf, u_int64_t t_offset)
{

	char		fn[MAX_STRING_LENGTH];
	FILE		*f;
	FILE		*test;
	long		byteswritten = 0;
	char		temp[32];
	u_int64_t	block = ((t_offset) / s->block_size);
	int			i = 1;

	//Name files based on their block offset
	needle->written = TRUE;

	if (get_mode(s, mode_write_audit))
		{
		if (needle->comment == NULL)
			strcpy(needle->comment, " ");

		audit_msg(s,
				  "%d:\t%10ld.%s \t %10s \t %10llu \t %s",
				  s->fileswritten,
				  block,
				  needle->suffix,
				  human_readable(len, temp),
				  t_offset,
				  needle->comment);
		s->fileswritten++;
		needle->found++;
		return TRUE;
		}

	snprintf(fn,
			 MAX_STRING_LENGTH,
			 "%s/%s/%0*llu.%s",
			 s->output_directory,
			 needle->suffix,
			 8,
			 block,
			 needle->suffix);

	test = fopen(fn, "r");
	while (test)	/*Test the files to make sure we have unique file names, some headers could be within the same block*/
		{
		memset(fn, 0, MAX_STRING_LENGTH - 1);
		snprintf(fn,
				 MAX_STRING_LENGTH - 1,
				 "%s/%s/%0*llu_%d.%s",
				 s->output_directory,
				 needle->suffix,
				 8,
				 block,
				 i,
				 needle->suffix);
		i++;
		fclose(test);
		test = fopen(fn, "r");
		}

	if (!(f = fopen(fn, "w")))
		{
		printf("fn = %s  failed\n", fn);
		fatal_error(s, "Can't open file for writing \n");
		}

	if ((byteswritten = fwrite(buf, sizeof(char), len, f)) != len)
		{
		fprintf(stderr, "fn=%s bytes=%lu\n", fn, byteswritten);
		fatal_error(s, "Error writing file\n");
		}

	if (fclose(f))
		{
		fatal_error(s, "Error closing file\n");
		}

	if (needle->comment == NULL)
		strcpy(needle->comment, " ");
	
	if (i == 1) {
      audit_msg(s,"%d:\t%08llu.%s \t %10s \t %10llu \t %s",
         s->fileswritten,
         block,
         needle->suffix,
         human_readable(len, temp),
         t_offset,
         needle->comment);
         } else {
      audit_msg(s,"%d:\t%08llu_%d.%s \t %10s \t %10llu \t %s",
         s->fileswritten,
         block,
         i - 1,
         needle->suffix, 
         human_readable(len, temp),
         t_offset,
         needle->comment);
         }

/*
	audit_msg(s,"%d:\t%10llu.%s \t %10s \t %10llu \t %s",
			  s->fileswritten,
			  block,
			  needle->suffix,
			  human_readable(len, temp),
			  t_offset,
			  needle->comment);

*/
	s->fileswritten++;
	needle->found++;
	return TRUE;
}
