
	 /* FOREMOST
 *
 * By Jesse Kornblum, Kris Kendall, & Nick Mikus
 *
 * This is a work of the US Government. In accordance with 17 USC 105,
 * copyright protection is not available for any work of the US Government.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "main.h"

int user_interrupt (f_state * s, f_info * i)
	{
	audit_msg(s, "Interrupt received at %s", current_time());

	/* RBF - Write user_interrupt */
	fclose(i->handle);
	free(s);
	free(i);
	cleanup_output(s);
	exit(-1);
	return FALSE;
	}

unsigned char *read_from_disk(u_int64_t offset, f_info *i, u_int64_t length)
{

	u_int64_t		bytesread = 0;
	unsigned char	*newbuf = (unsigned char *)malloc(length * sizeof(char));
	if (!newbuf) {
           fprintf(stderr, "Ran out of memory in read_from_disk()\n");
           exit(1);
         }

	fseeko(i->handle, offset, SEEK_SET);
	bytesread = fread(newbuf, 1, length, i->handle);
	if (bytesread != length)
	{
		free(newbuf);
		return NULL;
	}
	else
	{
		return newbuf;
	}
}

/*
   Perform a modified boyer-moore string search (w/ support for wildcards and case-insensitive searches)
   and allows the starting position in the buffer to be manually set, which allows data to be skipped
*/
unsigned char *bm_search_skipn(unsigned char *needle, size_t needle_len, unsigned char *haystack,
							   size_t haystack_len, size_t table[UCHAR_MAX + 1], int casesensitive,
							   int searchtype, int start_pos)
{
	register size_t shift = 0;
	register size_t pos = start_pos;
	unsigned char	*here;

	if (needle_len == 0)
		return haystack;

	if (searchtype == SEARCHTYPE_FORWARD || searchtype == SEARCHTYPE_FORWARD_NEXT)
		{
		while (pos < haystack_len)
			{
			while (pos < haystack_len && (shift = table[(unsigned char)haystack[pos]]) > 0)
				{
				pos += shift;
				}

			if (0 == shift)
				{
				here = (unsigned char *) &haystack[pos - needle_len + 1];
				if (0 == memwildcardcmp(needle, here, needle_len, casesensitive))
					{
					return (here);
					}
				else
					pos++;
				}
			}

		return NULL;
		}
	else if (searchtype == SEARCHTYPE_REVERSE)	//Run our search backwards
		{
		while (pos < haystack_len)
			{
			while
			(
				pos < haystack_len &&
				(shift = table[(unsigned char)haystack[haystack_len - pos - 1]]) > 0
			)
				{
				pos += shift;
				}

			if (0 == shift)
				{
				if (0 == memwildcardcmp(needle, here = (unsigned char *) &haystack[haystack_len - pos - 1],
					needle_len, casesensitive))
					{
					return (here);
					}
				else
					pos++;
				}
			}

		return NULL;
		}

	return NULL;
}

/*
   Perform a modified boyer-moore string search (w/ support for wildcards and case-insensitive searches)
   and allows the starting position in the buffer to be manually set, which allows data to be skipped
*/
unsigned char *bm_search(unsigned char *needle, size_t needle_len, unsigned char *haystack,
						 size_t haystack_len, size_t table[UCHAR_MAX + 1], int case_sen,
						 int searchtype)
{

	//printf("The needle2 is:\t");
	//printx(needle,0,needle_len);
	return bm_search_skipn(needle,
						   needle_len,
						   haystack,
						   haystack_len,
						   table,
						   case_sen,
						   searchtype,
						   needle_len - 1);

}

void setup_stream(f_state *s, f_info *i)
{
	char	buffer[MAX_STRING_LENGTH];
	u_int64_t	skip = (((u_int64_t) s->skip) * ((u_int64_t) s->block_size));
#ifdef DEBUG
	printf("s->skip=%d s->block_size=%d total=%llu\n",
		   s->skip,
		   s->block_size,
		   (((u_int64_t) s->skip) * ((u_int64_t) s->block_size)));
#endif
	i->bytes_read = 0;
	i->total_megs = i->total_bytes / ONE_MEGABYTE;

	if (i->total_bytes != 0)
		{
		audit_msg(s,
				  "Length: %s (%llu bytes)",
				  human_readable(i->total_bytes, buffer),
				  i->total_bytes);
		}
	else
		audit_msg(s, "Length: Unknown");

	if (s->skip != 0)
		{
		audit_msg(s, "Skipping: %s (%llu bytes)", human_readable(skip, buffer), skip);
		fseeko(i->handle, skip, SEEK_SET);
		if (i->total_bytes != 0)
			i->total_bytes -= skip;
		}

	audit_msg(s, " ");

#ifdef __WIN32
	i->last_read = 0;
	i->overflow_count = 0;
#endif

}

void audit_layout(f_state *s)
{
	audit_msg(s,
			  "Num\t %s (bs=%d)\t %10s\t %s\t %s \n",
			  "Name",
			  s->block_size,
			  "Size",
			  "File Offset",
			  "Comment");

}

void dumpInd(unsigned char *ind, int bs)
{
	int i = 0;
	printf("\n/*******************************/\n");

	while (bs > 0)
		{
		if (i % 10 == 0)
			printf("\n");

		//printx(ind,0,10);
		printf("%4u ", htoi(ind, FOREMOST_LITTLE_ENDIAN));

		bs -= 4;
		ind += 4;
		i++;
		}

	printf("\n/*******************************/\n");
}

/********************************************************************************
 *Function: ind_block
 *Description: check if the block foundat is pointing to looks like an indirect 
 *	block
 *Return: TRUE/FALSE
 **********************************************************************************/
int ind_block(unsigned char *foundat, u_int64_t buflen, int bs)
{

	unsigned char	*temp = foundat;
	int				jump = 12 * bs;
	unsigned int	block = 0;
	unsigned int	block2 = 0;
	unsigned int	dif = 0;
	int				i = 0;
	unsigned int	one = 1;
	unsigned int	numbers = (bs / 4) - 1;

	//int reconstruct=FALSE;

	/*Make sure we don't jump past the end of the buffer*/
	if (buflen < jump + 16)
		return FALSE;

	while (i < numbers)
		{
		block = htoi(&temp[jump + (i * 4)], FOREMOST_LITTLE_ENDIAN);

		if (block < 0)
			return FALSE;

		if (block == 0)
			{
			break;
			}

		i++;
		block2 = htoi(&temp[jump + (i * 4)], FOREMOST_LITTLE_ENDIAN);
		if (block2 < 0)
			return FALSE;

		if (block2 == 0)
			{
			break;
			}

		dif = block2 - block;

		if (dif == one)
		{

#ifdef DEBUG
			printf("block1:=%u, block2:=%u dif=%u\n", block, block2, dif);
#endif
		}
		else
		{

#ifdef DEBUG
			printf("Failure, dif!=1\n");
			printf("\tblock1:=%u, block2:=%u dif=%u\n", block, block2, dif);
#endif

			return FALSE;
		}

#ifdef DEBUG
		printf("block1:=%u, block2:=%u dif=%u\n", block, block2, dif);
#endif
		}

	if (i == 0)
		return FALSE;

	/*Check if the rest of the bytes are zero'd out */
	for (i = i + 1; i < numbers; i++)
		{
		block = htoi(&temp[jump + (i * 4)], FOREMOST_LITTLE_ENDIAN);
		if (block != 0)
			{

			//printf("Failure, 0 test\n");
			return FALSE;
			}
		}

	return TRUE;
}

/********************************************************************************
 *Function: search_chunk
 *Description: Analyze the given chunk by running each defined search spec on it
 *Return: TRUE/FALSE
 **********************************************************************************/
int search_chunk(f_state *s, unsigned char *buf, f_info *i, u_int64_t chunk_size, u_int64_t f_offset)
{

	u_int64_t		c_offset = 0;
	//u_int64_t               foundat_off = 0;
	//u_int64_t               buf_off = 0;

	unsigned char	*foundat = buf;
	unsigned char	*current_pos = NULL;
	unsigned char	*header_pos = NULL;
	unsigned char	*newbuf = NULL;
	unsigned char	*ind_ptr = NULL;
	u_int64_t		current_buflen = chunk_size;
	int				tryBS[3] = { 4096, 1024, 512 };
	unsigned char	*extractbuf = NULL;
	u_int64_t		file_size = 0;
	s_spec			*needle = NULL;
	int				j = 0;
	int				bs = 0;
	int				rem = 0;
	int				x = 0;
	int				found_ind = FALSE;
	 off_t saveme;
	//char comment[32];
	for (j = 0; j < s->num_builtin; j++)
		{
		needle = &search_spec[j];
		foundat = buf;										/*reset the buffer for the next search spec*/
#ifdef DEBUG
		printf("	SEARCHING FOR %s's\n", needle->suffix);
#endif
		bs = 0;
		current_buflen = chunk_size;
		while (foundat)
			{
			needle->written = FALSE;
			found_ind = FALSE;
			memset(needle->comment, 0, COMMENT_LENGTH - 1);
                        if (chunk_size <= (foundat - buf)) {
#ifdef DEBUG
				printf("avoided seg fault in search_chunk()\n");
#endif
				foundat = NULL;
				break;
			}
			current_buflen = chunk_size - (foundat - buf);

			//if((foundat-buf)< 1 ) break;	
#ifdef DEBUG
			//foundat_off=foundat;
			//buf_off=buf;
			//printf("current buf:=%llu (foundat-buf)=%llu \n", current_buflen, (u_int64_t) (foundat_off - buf_off));
#endif
			if (signal_caught == SIGTERM || signal_caught == SIGINT)
				{
				user_interrupt(s, i);
				printf("Cleaning up.\n");
				signal_caught = 0;
				}

			if (get_mode(s, mode_quick))					/*RUN QUICK SEARCH*/
			{
#ifdef DEBUG

				//printf("quick mode is on\n");
#endif

				/*Check if we are not on a block head, adjust if so*/
				rem = (foundat - buf) % s->block_size;
				if (rem != 0)
					{
					foundat += (s->block_size - rem);
					}

				if (memwildcardcmp(needle->header, foundat, needle->header_len, needle->case_sen
					) != 0)
					{

					/*No match, jump to the next block*/
					if (current_buflen > s->block_size)
						{
						foundat += s->block_size;
						continue;
						}
					else									/*We are out of buffer lets go to the next search spec*/
						{
						foundat = NULL;
						break;
						}
					}

				header_pos = foundat;
			}
			else											/**********RUN STANDARD SEARCH********************/
				{
				foundat = bm_search(needle->header,
									needle->header_len,
									foundat,
									current_buflen,			//How much to search through
									needle->header_bm_table,
									needle->case_sen,		//casesensative
									SEARCHTYPE_FORWARD);

				header_pos = foundat;
				}

			if (foundat != NULL && foundat >= 0)			/*We got something, run the appropriate heuristic to find the EOF*/
				{
				current_buflen = chunk_size - (foundat - buf);

				if (get_mode(s, mode_ind_blk))
				{
#ifdef DEBUG
					printf("ind blk detection on\n");
#endif

					//dumpInd(foundat+12*1024,1024);
					for (x = 0; x < 3; x++)
						{
						bs = tryBS[x];

						if (ind_block(foundat, current_buflen, bs))
							{
							if (get_mode(s, mode_verbose))
								{
								sprintf(needle->comment, " (IND BLK bs:=%d)", bs);
								}

							//dumpInd(foundat+12*bs,bs);
#ifdef DEBUG
							printf("performing mem move\n");
#endif
							if(current_buflen >  13 * bs)//Make sure we have enough buffer
								{
								if (!memmove(foundat + 12 * bs, foundat + 13 * bs, current_buflen - 13 * bs))
								break;

								found_ind = TRUE;
#ifdef DEBUG
								printf("performing mem move complete\n");
#endif
								ind_ptr = foundat + 12 * bs;
								current_buflen -= bs;
								chunk_size -= bs;
								break;
								}
							}

						}

				}

				c_offset = (foundat - buf);
				current_pos = foundat;

				/*Now lets analyze the file and see if we can determine its size*/

				// printf("c_offset=%llu %x %x %llx\n", c_offset,foundat,buf,c_offset);
				foundat = extract_file(s, c_offset, foundat, current_buflen, needle, f_offset);
#ifdef DEBUG
				if (foundat == NULL)
					{
					printf("Foundat == NULL!!!\n");
					}
#endif
				if (get_mode(s, mode_write_all))
					{
					if (needle->written == FALSE)
						{

						/*write every header we find*/
						if (current_buflen >= needle->max_len)
							{
							file_size = needle->max_len;
							}
						else
							{
							file_size = current_buflen;
							}

						sprintf(needle->comment, " (Header dump)");
						extractbuf = (unsigned char *)malloc(file_size * sizeof(char));
						memcpy(extractbuf, header_pos, file_size);
						write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
						free(extractbuf);
						}
					}
				else if (!foundat)							/*Should we search further?*/
					{

					/*We couldn't determine where the file ends, now lets check to see
			* if we should try again
			*/
					if (current_buflen < needle->max_len)	/*We need to bridge the gap*/
					{
#ifdef DEBUG
						printf("	Bridge the gap\n");
#endif
						saveme = ftello(i->handle);
						/*grow the buffer and try to extract again*/
						newbuf = read_from_disk(c_offset + f_offset, i, needle->max_len);
						if (newbuf == NULL)
							break;
						current_pos = extract_file(s,
												   c_offset,
												   newbuf,
												   needle->max_len,
												   needle,
												   f_offset);
						
						/*Lets put the fp back*/
						fseeko(i->handle, saveme, SEEK_SET);
						

						free(newbuf);
					}
					else
						{
						foundat = header_pos;				/*reset the foundat pointer to the location of the last header*/
						foundat += needle->header_len + 1;	/*jump past the header*/
						}
					}


				}

			if (found_ind)
				{

				/*Put the ind blk back in, re-arrange the buffer so that the future blks names come out correct*/
#ifdef DEBUG
						printf("Replacing the ind block\n");
#endif
				/*This is slow, should we do this??????*/
				if (!memmove(ind_ptr + 1 * bs, ind_ptr, current_buflen - 13 * bs))
					break;
				memset(ind_ptr, 0, bs - 1);
				chunk_size += bs;
				memset(needle->comment, 0, COMMENT_LENGTH - 1);
				}
			}	//end while
		}

	return TRUE;
}

/********************************************************************************
 *Function: search_stream
 *Description: Analyze the file by reading 1 chunk (default: 100MB) at a time and 
 *passing it to	search_chunk
 *Return: TRUE/FALSE
 **********************************************************************************/
int search_stream(f_state *s, f_info *i)
{
	u_int64_t		bytesread = 0;
	u_int64_t		f_offset = 0;
	u_int64_t		chunk_size = ((u_int64_t) s->chunk_size) * MEGABYTE;
	unsigned char	*buf = (unsigned char *)malloc(sizeof(char) * chunk_size);

	setup_stream(s, i);

	audit_layout(s);
#ifdef DEBUG
	printf("\n\t READING THE FILE INTO MEMORY\n");
#endif

	while ((bytesread = fread(buf, 1, chunk_size, i->handle)) > 0)
		{
		if (signal_caught == SIGTERM || signal_caught == SIGINT)
			{
			user_interrupt(s, i);
			printf("Cleaning up.\n");
			signal_caught = 0;
			}

#ifdef DEBUG
		printf("\n\tbytes_read:=%llu\n", bytesread);
#endif
		search_chunk(s, buf, i, bytesread, f_offset);
		f_offset += bytesread;
		if (!get_mode(s, mode_quiet))
			{
			fprintf(stderr, "*");

			//displayPosition(s,i,f_offset);
			}

		/*FIX ME***
	* We should jump back and make sure we didn't miss any headers that are 
	* bridged between chunks.  What is the best way to do this?\
  	*/
		}

	if (!get_mode(s, mode_quiet))
		{
		fprintf(stderr, "|\n");
		}

#ifdef DEBUG
	printf("\n\tDONE READING bytes_read:=%llu\n", bytesread);
#endif
	if (signal_caught == SIGTERM || signal_caught == SIGINT)
		{
		user_interrupt(s, i);
		printf("Cleaning up.\n");
		signal_caught = 0;
		}

	free(buf);
	return FALSE;
}

void audit_start(f_state *s, f_info *i)
{
	if (!get_mode(s, mode_quiet))
		{
		fprintf(stderr, "Processing: %s\n|", i->file_name);
		}

	audit_msg(s, FOREMOST_DIVIDER);
	audit_msg(s, "File: %s", i->file_name);
	audit_msg(s, "Start: %s", current_time());
}

void audit_finish(f_state *s, f_info *i)
{
	audit_msg(s, "Finish: %s", current_time());
}

int process_file(f_state *s)
{

	//printf("processing file\n");
	f_info	*i = (f_info *)malloc(sizeof(f_info));
	char	temp[PATH_MAX];

	if ((realpath(s->input_file, temp)) == NULL)
		{
		print_error(s, s->input_file, strerror(errno));
		return TRUE;
		}

	i->file_name = strdup(s->input_file);
	i->is_stdin = FALSE;
	audit_start(s, i);

	//  printf("opening file %s\n",i->file_name);
#if defined(__LINUX)
	#ifdef DEBUG
	printf("Using 64 bit fopen\n");
	#endif
	i->handle = fopen64(i->file_name, "rb");
#elif defined(__WIN32)

	/*I would like to be able to read from
	* physical devices in Windows, have played
	* with different options to fopen and the
	* dd src says you need write access on WinXP
	* but nothing seems to work*/
	i->handle = fopen(i->file_name, "rb");
#else
	i->handle = fopen(i->file_name, "rb");
#endif
	if (i->handle == NULL)
		{
		print_error(s, s->input_file, strerror(errno));
		audit_msg(s, "Error: %s", strerror(errno));
		return TRUE;
		}

	i->total_bytes = find_file_size(i->handle);
	search_stream(s, i);
	audit_finish(s, i);

	fclose(i->handle);
	free(i);
	return FALSE;
}

int process_stdin(f_state *s)
{
	f_info	*i = (f_info *)malloc(sizeof(f_info));

	i->file_name = strdup("stdin");
	s->input_file = "stdin";
	i->handle = stdin;
	i->is_stdin = TRUE;

	/* We can't compute the size of this stream, we just ignore it*/
	i->total_bytes = 0;
	audit_start(s, i);

	search_stream(s, i);

	free(i->file_name);
	free(i);
	return FALSE;
}
