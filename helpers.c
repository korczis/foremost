
	 /* MD5DEEP - helpers.c
 *
 * By Jesse Kornblum
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

/* Removes any newlines at the end of the string buf.
   Works for both *nix and Windows styles of newlines.
   Returns the new length of the string. */
unsigned int chop (char *buf)
	{

	/* Windows newlines are 0x0d 0x0a, *nix are 0x0a */
	unsigned int	len = strlen(buf);
	if (buf[len - 1] == 0x0a)
		{
		if (buf[len - 2] == 0x0d)
			{
			buf[len - 2] = buf[len - 1];
			}
		buf[len - 1] = buf[len];
		}
	return strlen(buf);
	}

char *units(unsigned int c)
{
	switch (c)
		{
		case 0:		return "B";
		case 1:		return "KB";
		case 2:		return "MB";
		case 3:		return "GB";
		case 4:		return "TB";
		case 5:		return "PB";
		case 6:		return "EB";
		/* Steinbach's Guideline for Systems Programming:
       Never test for an error condition you don't know how to handle.

       Granted, given that no existing system can handle anything 
       more than 18 exabytes, this shouldn't be an issue. But how do we
       communicate that 'this shouldn't happen' to the user? */
		default:	return "??";
		}
}

char *human_readable(off_t size, char *buffer)
{
	unsigned int	count = 0;
	while (size > 1024)
		{
		size /= 1024;
		++count;
		}

	/* The size will be, at most, 1023, and the units will be
     two characters no matter what. Thus, the maximum length of
     this string is six characters. e.g. strlen("1023 EB") = 6 */
	if (sizeof(off_t) == 4)
		{
		snprintf(buffer, 8, "%u %s", (unsigned int)size, units(count));
		}
	else if (sizeof(off_t) == 8)
		{
		snprintf(buffer, 8, "%llu %s", (u_int64_t) size, units(count));
		}

	return buffer;
}

char *current_time(void)
{
	time_t	now = time(NULL);
	char	*ascii_time = ctime(&now);
	chop(ascii_time);
	return ascii_time;
}

/* Shift the contents of a string so that the values after 'new_start'
   will now begin at location 'start' */
void shift_string(char *fn, int start, int new_start)
{
	if (start < 0 || start > strlen(fn) || new_start < 0 || new_start < start)
		return;

	while (new_start < strlen(fn))
		{
		fn[start] = fn[new_start];
		new_start++;
		start++;
		}

	fn[start] = 0;
}

void make_magic(void)
{
	printf("%s%s",
		   "\x53\x41\x4E\x20\x44\x49\x4D\x41\x53\x20\x48\x49\x47\x48\x20\x53\x43\x48\x4F\x4F\x4C\x20\x46\x4F\x4F\x54\x42\x41\x4C\x4C\x20\x52\x55\x4C\x45\x53\x21",
	   NEWLINE);
}

#if defined(__UNIX)

/* Return the size, in bytes of an open file stream. On error, return 0 */
	#if defined(__LINUX)

off_t find_file_size(FILE *f)
{
	off_t		num_sectors = 0;
	int			fd = fileno(f);
	struct stat sb;

	if (fstat(fd, &sb))
		{
		return 0;
		}

	if (S_ISREG(sb.st_mode) || S_ISDIR(sb.st_mode))
		return sb.st_size;
	else if (S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode))
		{
		if (ioctl(fd, BLKGETSIZE, &num_sectors))
		{
		#if defined(__DEBUG)
			fprintf(stderr, "%s: ioctl call to BLKGETSIZE failed.%s", __progname, NEWLINE);
		#endif
		}
		else
			return (num_sectors * 512);
		}

	return 0;
}

	#elif defined(__MACOSX)

		#include <stdint.h>
		#include <sys/ioctl.h>
		#include <sys/disk.h>

off_t find_file_size(FILE *f)
{
		#ifdef DEBUG
	printf("	FIND MAC file size\n");
		#endif
	return 0;	/*FIX ME this function causes strange problems on MACOSX, so for now return 0*/
	struct stat info;
	off_t		total = 0;
	off_t		original = ftello(f);
	int			ok = TRUE, fd = fileno(f);

	/* I'd prefer not to use fstat as it will follow symbolic links. We don't
     follow symbolic links. That being said, all symbolic links *should*
     have been caught before we got here. */
	fstat(fd, &info);

	/* Block devices, like /dev/hda, don't return a normal filesize.
     If we are working with a block device, we have to ask the operating
     system to tell us the true size of the device. 
     
     The following only works on Linux as far as I know. If you know
     how to port this code to another operating system, please contact
     the current maintainer of this program! */
	if (S_ISBLK(info.st_mode))
		{
		daddr_t blocksize = 0;
		daddr_t blockcount = 0;

		/* Get the block size */
		if (ioctl(fd, DKIOCGETBLOCKSIZE, blocksize) < 0)
			{
			ok = FALSE;
		#if defined(__DEBUG)
			perror("DKIOCGETBLOCKSIZE failed");
		#endif
			}

		/* Get the number of blocks */
		if (ok)
			{
			if (ioctl(fd, DKIOCGETBLOCKCOUNT, blockcount) < 0)
			{
		#if defined(__DEBUG)
				perror("DKIOCGETBLOCKCOUNT failed");
		#endif
			}
			}

		total = blocksize * blockcount;

		}

	else
		{

		/* I don't know why, but if you don't initialize this value you'll
       get wildly innacurate results when you try to run this function */
		if ((fseeko(f, 0, SEEK_END)))
			return 0;
		total = ftello(f);
		if ((fseeko(f, original, SEEK_SET)))
			return 0;
		}

	return (total - original);
}

	#else

/* This is code for general UNIX systems 
   (e.g. NetBSD, FreeBSD, OpenBSD, etc) */
static off_t midpoint(off_t a, off_t b, long blksize)
{
	off_t	aprime = a / blksize;
	off_t	bprime = b / blksize;
	off_t	c, cprime;

	cprime = (bprime - aprime) / 2 + aprime;
	c = cprime * blksize;

	return c;
}

off_t find_dev_size(int fd, int blk_size)
{

	off_t	curr = 0, amount = 0;
	void	*buf;

	if (blk_size == 0)
		return 0;

	buf = malloc(blk_size);

	for (;;)
		{
		ssize_t nread;

		lseek(fd, curr, SEEK_SET);
		nread = read(fd, buf, blk_size);
		if (nread < blk_size)
			{
			if (nread <= 0)
				{
				if (curr == amount)
					{
					free(buf);
					lseek(fd, 0, SEEK_SET);
					return amount;
					}

				curr = midpoint(amount, curr, blk_size);
				}
			else
				{	/* 0 < nread < blk_size */
				free(buf);
				lseek(fd, 0, SEEK_SET);
				return amount + nread;
				}
			}
		else
			{
			amount = curr + blk_size;
			curr = amount * 2;
			}
		}

	free(buf);
	lseek(fd, 0, SEEK_SET);
	return amount;
}

off_t find_file_size(FILE *f)
{
	int			fd = fileno(f);
	struct stat sb;
	return 0;		/*FIX ME SOLARIS FILE SIZE CAUSES SEG FAULT, for now just return 0*/

	if (fstat(fd, &sb))
		return 0;

	if (S_ISREG(sb.st_mode) || S_ISDIR(sb.st_mode))
		return sb.st_size;
	else if (S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode))
		return find_dev_size(fd, sb.st_blksize);

	return 0;
}

	#endif /* UNIX Flavors */
#endif /* ifdef __UNIX */

#if defined(__WIN32)
off_t find_file_size(FILE *f)
{
	off_t	total = 0, original = ftello(f);

	if ((fseeko(f, 0, SEEK_END)))
		return 0;

	total = ftello(f);
	if ((fseeko(f, original, SEEK_SET)))
		return 0;

	return total;
}

#endif /* ifdef __WIN32 */

void print_search_specs(f_state *s)
{
	int i = 0;
	int j = 0;
	printf("\nDUMPING BUILTIN SEARCH INFO\n\t");
	for (i = 0; i < s->num_builtin; i++)
		{

		printf("%s:\n\t footer_len:=%d, header_len:=%d, max_len:=%llu ",
			   search_spec[i].suffix,
			   search_spec[i].footer_len,
			   search_spec[i].header_len,
			   search_spec[i].max_len);
		printf("\n\t header:\t");
		printx(search_spec[i].header, 0, search_spec[i].header_len);
		printf("\t footer:\t");
		printx(search_spec[i].footer, 0, search_spec[i].footer_len);
		for (j = 0; j < search_spec[i].num_markers; j++)
			{
			printf("\tmarker: \t");
			printx(search_spec[i].markerlist[j].value, 0, search_spec[i].markerlist[j].len);
			}

		}

}

void print_stats(f_state *s)
{
	int i = 0;
	audit_msg(s, "\n%d FILES EXTRACTED\n\t", s->fileswritten);
	for (i = 0; i < s->num_builtin; i++)
		{

		if (search_spec[i].found != 0)
			{
			if (search_spec[i].type == OLE)
				search_spec[i].suffix = "ole";
			else if (search_spec[i].type == RIFF)
				search_spec[i].suffix = "rif";
			else if (search_spec[i].type == ZIP)
				search_spec[i].suffix = "zip";
			audit_msg(s, "%s:= %d", search_spec[i].suffix, search_spec[i].found);
			}
		}
}

int charactersMatch(char a, char b, int caseSensitive)
{

	//if(a==b) return 1;
	if (a == wildcard || a == b)
		return 1;
	if (caseSensitive || (a < 'A' || a > 'z' || b < 'A' || b > 'z'))
		return 0;

	/* This line is equivalent to (abs(a-b)) == 'a' - 'A' */
	return (abs(a - b) == 32);
}

int memwildcardcmp(const void *s1, const void *s2, size_t n, int caseSensitive)
{
	if (n != 0)
		{
		register const unsigned char	*p1 = s1, *p2 = s2;
		do
			{
			if (!charactersMatch(*p1++, *p2++, caseSensitive))
				return (*--p1 -*--p2);
			}
		while (--n != 0);
		}

	return (0);
}

void printx(unsigned char *buf, int start, int end)
{
	int i = 0;
	for (i = start; i < end; i++)
		{
		printf("%x ", buf[i]);
		}

	printf("\n");
}

char *reverse_string(char *to, char *from, int startLocation, int endLocation)
{
	int i = endLocation;
	int j = 0;
	for (j = startLocation; j < endLocation; j++)
		{
		i--;
		to[j] = from[i];
		}

	return to;
}

unsigned short htos(unsigned char s[], int endian)
{

	unsigned char	*bytes = (unsigned char *)malloc(sizeof(unsigned short) * sizeof(char));
	unsigned short	size = 0;
	char			temp = 'x';
	bytes = memcpy(bytes, s, sizeof(short));

	if (endian == FOREMOST_BIG_ENDIAN && BYTE_ORDER == LITTLE_ENDIAN)
		{

		//printf("switching the byte order\n");
		temp = bytes[0];
		bytes[0] = bytes[1];
		bytes[1] = temp;

		}
	else if (endian == FOREMOST_LITTLE_ENDIAN && BYTE_ORDER == BIG_ENDIAN)
		{
		temp = bytes[0];
		bytes[0] = bytes[1];
		bytes[1] = temp;
		}

	size = *((unsigned short *)bytes);
	free(bytes);
	return size;
}

unsigned int htoi(unsigned char s[], int endian)
{

	int				length = sizeof(int);
	unsigned char	*bytes = (unsigned char *)malloc(length * sizeof(char));
	unsigned int	size = 0;

	bytes = memcpy(bytes, s, length);

	if (endian == FOREMOST_BIG_ENDIAN && BYTE_ORDER == LITTLE_ENDIAN)
		{

		bytes = (unsigned char *)reverse_string((char *)bytes, (char *)s, 0, length);
		}
	else if (endian == FOREMOST_LITTLE_ENDIAN && BYTE_ORDER == BIG_ENDIAN)
		{

		bytes = (unsigned char *)reverse_string((char *)bytes, (char *)s, 0, length);
		}

	size = *((unsigned int *)bytes);

	free(bytes);
	return size;
}

u_int64_t htoll(unsigned char s[], int endian)
{
	int				length = sizeof(u_int64_t);
	unsigned char	*bytes = (unsigned char *)malloc(length * sizeof(char));
	u_int64_t	size = 0;
	bytes = memcpy(bytes, s, length);
#ifdef DEBUG
	printf("htoll len=%d endian=%d\n",length,endian);
#endif	
	if (endian == FOREMOST_BIG_ENDIAN && BYTE_ORDER == LITTLE_ENDIAN)
		{
#ifdef DEBUG
		printf("reverse0\n");
#endif
		bytes = (unsigned char *)reverse_string((char *)bytes, (char *)s, 0, length);
		}
	else if (endian == FOREMOST_LITTLE_ENDIAN && BYTE_ORDER == BIG_ENDIAN)
		{
#ifdef DEBUG
	printf("reverse1\n");
#endif
		bytes = (unsigned char *)reverse_string((char *)bytes, (char *)s, 0, length);
		}

	size = *((u_int64_t *)bytes);
#ifdef DEBUG
	printf("htoll size=%llu\n",size);
	printx(bytes,0,length);
#endif	
	

	free(bytes);
	return size;
}

/* display Position: Tell the user how far through the infile we are */
int displayPosition(f_state *s, f_info *i, u_int64_t pos)
{

	int			percentDone = 0;
	static int	last_val = 0;
	int			count;
	int			flag = FALSE;
	int			factor = 4;
	int			multiplier = 25;
	int			number_of_stars = 0;
	char		buffer[256];
	long double skip = s->skip * s->block_size;

	long double tot_bytes = (long double)((i->total_bytes));
	tot_bytes -= skip;
	if (i->total_bytes > 0)
		{
		percentDone = (((long double)pos) / ((long double)tot_bytes)) * 100;
		if (percentDone != last_val)
			flag = TRUE;
		last_val = percentDone;
		}
	else
		{
		flag = TRUE;
		factor = 4;
		multiplier = 25;
		}

	if (flag)
		{
		number_of_stars = percentDone / factor;

		printf("%s: |", s->input_file);
		for (count = 0; count < number_of_stars; count++)
			{
			printf("*");
			}

		for (count = 0; count < (multiplier - number_of_stars); count++)
			{
			printf(" ");
			}

		if (i->total_bytes > 0)
			{
			printf("|\t %d%% done\n", percentDone);
			}
		else
			{
			printf("|\t %s done\n", human_readable(pos, buffer));

			}
		}

	if (percentDone == 100)
		{
		last_val = 0;
		}

	return TRUE;
}
