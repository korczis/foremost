/*
	Modified API from http://chicago.sourceforge.net/devel/docs/ole/
	Basically the same API, added error checking and the ability
	to check buffers for docs, not just files.
*/
#include "main.h"
#include "ole.h"

/*Some ugly globals
* This API should be re-written
* in a modular fashion*/
unsigned char	buffer[OUR_BLK_SIZE];
char			*extract_name;
int				extract = 0;
int				dir_count = 0;
int				*FAT;
int				verbose = TRUE;
int				FATblk;
int				currFATblk;
int				highblk = 0;
int				block_list[OUR_BLK_SIZE / sizeof(int)];
extern int		errno;

/*Inititialize those globals used by extract_ole*/
void init_ole()
{
	int i = 0;
	extract = 0;
	dir_count = 0;
	FAT = NULL;
	highblk = 0;
	FATblk = 0;
	currFATblk = -1;
	dirlist = NULL;
	dl = NULL;
	for (i = 0; i < OUR_BLK_SIZE / sizeof(int); i++)
		{
		block_list[i] = 0;
		}

	for (i = 0; i < OUR_BLK_SIZE; i++)
		{
		buffer[i] = 0;
		}
}

void *Malloc(size_t bytes)
{
	void	*x;

	x = malloc(bytes);
	if (x)
		return x;
	die("Can't malloc %d bytes.\n", (char *)bytes);
	return 0;
}

void die(char *fmt, void *arg)
{
	fprintf(stderr, fmt, arg);
	exit(1);
}

int get_dir_block(unsigned char *fd, int blknum, int buffersize)
{
	int				i;
	struct OLE_DIR	*dir;
	unsigned char	*dest = NULL;

	dest = get_ole_block(fd, blknum, buffersize);
	if (dest == NULL)
		{
		return FALSE;
		}

	for (i = 0; i < DIRS_PER_BLK; i++)
		{
		dir = (struct OLE_DIR *) &dest[sizeof(struct OLE_DIR) * i];
		if (dir->type == NO_ENTRY)
			break;
		}

	if (i == DIRS_PER_BLK)
		{
		return TRUE;
		}
	else
		{
		return SHORT_BLOCK;
		}
}

int get_dir_info(unsigned char *src)
{
	int				i, j;
	char			*p, *q;
	struct OLE_DIR	*dir;
	int				punctCount = 0;
	short			name_size = 0;

	for (i = 0; i < DIRS_PER_BLK; i++)
		{
		dir = (struct OLE_DIR *) &src[sizeof(struct OLE_DIR) * i];
		punctCount = 0;

		//if(dir->reserved!=0) return FALSE;
		if (dir->type < 0)	//Should we check if values are > 5 ?????
		{
#ifdef DEBUG
			printf("\n	Invalid directory type\n");
			printf("type:=%c size:=%lu \n", dir->type, dir->size);
#endif
			return FALSE;
		}

		if (dir->type == NO_ENTRY)
			break;

#ifdef DEBUG

		//dump_dirent (i);
#endif
		dl = &dirlist[dir_count++];
		if (dl == NULL)
		{
#ifdef DEBUG
			printf("dl==NULL!!! bailing out\n");
#endif
			return FALSE;
		}

		if (dir_count > 500)
			return FALSE;	/*SANITY CHECKING*/
		q = dl->name;
		p = dir->name;

		name_size = htos((unsigned char *) &dir->namsiz, FOREMOST_LITTLE_ENDIAN);

#ifdef DEBUG
		printf(" dir->namsiz:=%d\n", name_size);
#endif
		if (name_size > 64 || name_size <= 0)
			return FALSE;

		if (*p < ' ')
			p += 2;			/* skip leading short */
		for (j = 0; j < name_size; j++, p++)
			{

			if (p == NULL || q == NULL)
				return FALSE;
			if (*p && isprint(*p))
				{

				if (ispunct(*p))
					punctCount++;
				*q++ = *p;

				}
			}

		if (punctCount > 3)
		{
#ifdef DEBUG
			printf("dl->name:=%s\n", dl->name);
			printf("pcount > 3!!! bailing out\n");
#endif
			return FALSE;
		}

		if (dl->name == NULL)
		{
#ifdef DEBUG
			printf("	***NULL dir name. bailing out \n");
#endif
			return FALSE;
		}

		/*Ignore Catalogs*/
		if (strstr(dl->name, "Catalog"))
			return FALSE;
		*q = 0;
		dl->type = dir->type;
		dl->size = htoi((unsigned char *) &dir->size, FOREMOST_LITTLE_ENDIAN);

		dl->start_block = htoi((unsigned char *) &dir->start_block, FOREMOST_LITTLE_ENDIAN);
		dl->next = htoi((unsigned char *) &dir->next_dirent, FOREMOST_LITTLE_ENDIAN);
		dl->prev = htoi((unsigned char *) &dir->prev_dirent, FOREMOST_LITTLE_ENDIAN);
		dl->dir = htoi((unsigned char *) &dir->dir_dirent, FOREMOST_LITTLE_ENDIAN);
		if (dir->type != STREAM)
			{
			dl->s1 = dir->secs1;
			dl->s2 = dir->secs2;
			dl->d1 = dir->days1;
			dl->d2 = dir->days2;
			}
		}

	return TRUE;
}

static int	*lnlv;			/* last next link visited ! */
int reorder_dirlist(struct DIRECTORY *dir, int level)
{

	//printf("	Reordering the dirlist\n");
	dir->level = level;
	if (dir->dir != -1 || dir->dir > dir_count)
		{
		return 0;
		}
	else if (!reorder_dirlist(&dirlist[dir->dir], level + 1))
		return 0;

	/* reorder next-link subtree, saving the most next link visited */
	if (dir->next != -1)
		{
		if (dir->next > dir_count)
			return 0;
		else if (!reorder_dirlist(&dirlist[dir->next], level))
			return 0;
		}
	else
		lnlv = &dir->next;

	/* move the prev child to the next link and reorder it, if any exist
 */
	if (dir->prev != -1)
		{
		if (dir->prev > dir_count)
			return 0;
		else
			{
			*lnlv = dir->prev;
			dir->prev = -1;
			if (!reorder_dirlist(&dirlist[*lnlv], level))
				return 0;
			}
		}

	return 1;
}

int get_block(unsigned char *fd, int blknum, unsigned char *dest, long long int buffersize)
{
	unsigned char		*temp = fd;
	int					i = 0;
	unsigned long long	jump = (unsigned long long)OUR_BLK_SIZE * (unsigned long long)(blknum + 1);
	if (blknum < -1 || jump < 0 || blknum > buffersize || buffersize < jump)
	{
#ifdef DEBUG
		printf("	Bad blk read1 blknum:=%d  jump:=%lld buffersize=%lld\n", blknum, jump, buffersize);
#endif
		return FALSE;
	}

	temp = fd + jump;
#ifdef DEBUG
	printf("	Jumping to %lld blknum=%d buffersize=%lld\n", jump, blknum, buffersize);
#endif
	for (i = 0; i < OUR_BLK_SIZE; i++)
		{
		dest[i] = temp[i];
		}

	if ((blknum + 1) > highblk)
		highblk = blknum + 1;
	return TRUE;
}

unsigned char *get_ole_block(unsigned char *fd, int blknum, unsigned long long buffersize)
{
	unsigned long long	jump = (unsigned long long)OUR_BLK_SIZE * (unsigned long long)(blknum + 1);
	if (blknum < -1 || jump < 0 || blknum > buffersize || buffersize < jump)
	{
#ifdef DEBUG
		printf("	Bad blk read1 blknum:=%d  jump:=%lld buffersize=%lld\n", blknum, jump, buffersize);
#endif
		return FALSE;
	}

#ifdef DEBUG
	printf("	Jumping to %lld blknum=%d buffersize=%lld\n", jump, blknum, buffersize);
#endif
	return (fd + jump);
}

int get_FAT_block(unsigned char *fd, int blknum, int *dest, int buffersize)
{
	static int	FATblk;

	//   static int currFATblk = -1;
	FATblk = htoi((unsigned char *) &FAT[blknum / (OUR_BLK_SIZE / sizeof(int))],
				  FOREMOST_LITTLE_ENDIAN);
#ifdef DEBUG
	printf("****blknum:=%d FATblk:=%d currFATblk:=%d\n", blknum, FATblk, currFATblk);
#endif
	if (currFATblk != FATblk)
	{
#ifdef DEBUG
		printf("*****blknum:=%d FATblk:=%d\n", blknum, FATblk);
#endif
		if (!get_block(fd, FATblk, (unsigned char *)dest, buffersize))
			{
			return FALSE;
			}

		currFATblk = FATblk;
	}

	return TRUE;
}

void dump_header(struct OLE_HDR *h)
{
	int i, *x;

	//struct OLE_HDR *h = (struct OLE_HDR *) buffer;
	// fprintf (stderr, "clsid  = ");
	//printx(h->clsid,0,16);
	fprintf(stderr,
			"\nuMinorVersion  = %u\t",
			htos((unsigned char *) &h->uMinorVersion, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uDllVersion  = %u\t",
			htos((unsigned char *) &h->uDllVersion, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uByteOrder  = %u\n",
			htos((unsigned char *) &h->uByteOrder, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uSectorShift  = %u\t",
			htos((unsigned char *) &h->uSectorShift, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uMiniSectorShift  = %u\t",
			htos((unsigned char *) &h->uMiniSectorShift, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved  = %u\n",
			htos((unsigned char *) &h->reserved, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved1  = %u\t",
			htoi((unsigned char *) &h->reserved1, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved2  = %u\t",
			htoi((unsigned char *) &h->reserved2, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"csectMiniFat = %u\t",
			htoi((unsigned char *) &h->csectMiniFat, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"miniSectorCutoff = %u\n",
			htoi((unsigned char *) &h->miniSectorCutoff, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"root_start_block  = %u\n",
			htoi((unsigned char *) &h->root_start_block, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"dir flag = %u\n",
			htoi((unsigned char *) &h->dir_flag, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"# FAT blocks = %u\n",
			htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"FAT_next_block = %u\n",
			htoi((unsigned char *) &h->FAT_next_block, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"# extra FAT blocks = %u\n",
			htoi((unsigned char *) &h->num_extra_FAT_blocks, FOREMOST_LITTLE_ENDIAN));
	x = (int *) &h[1];
	fprintf(stderr, "bbd list:");
	for (i = 0; i < 109; i++, x++)
		{
		if ((i % 10) == 0)
			fprintf(stderr, "\n");
		if (*x == '\xff')
			break;
		fprintf(stderr, "%x ", *x);
		}

	fprintf(stderr, "\n	**************End of header***********\n");
}

struct OLE_HDR *reverseBlock(struct OLE_HDR *dest, struct OLE_HDR *h)
{
	int i, *x, *y;
	dest->uMinorVersion = htos((unsigned char *) &h->uMinorVersion, FOREMOST_LITTLE_ENDIAN);
	dest->uDllVersion = htos((unsigned char *) &h->uDllVersion, FOREMOST_LITTLE_ENDIAN);
	dest->uByteOrder = htos((unsigned char *) &h->uByteOrder, FOREMOST_LITTLE_ENDIAN);				/*28*/
	dest->uSectorShift = htos((unsigned char *) &h->uSectorShift, FOREMOST_LITTLE_ENDIAN);
	dest->uMiniSectorShift = htos((unsigned char *) &h->uMiniSectorShift, FOREMOST_LITTLE_ENDIAN);	/*32*/
	dest->reserved = htos((unsigned char *) &h->reserved, FOREMOST_LITTLE_ENDIAN);					/*34*/
	dest->reserved1 = htoi((unsigned char *) &h->reserved1, FOREMOST_LITTLE_ENDIAN);				/*36*/
	dest->reserved2 = htoi((unsigned char *) &h->reserved2, FOREMOST_LITTLE_ENDIAN);				/*40*/
	dest->num_FAT_blocks = htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN);		/*44*/
	dest->root_start_block = htoi((unsigned char *) &h->root_start_block, FOREMOST_LITTLE_ENDIAN);	/*48*/
	dest->dfsignature = htoi((unsigned char *) &h->dfsignature, FOREMOST_LITTLE_ENDIAN);			/*52*/
	dest->miniSectorCutoff = htoi((unsigned char *) &h->miniSectorCutoff, FOREMOST_LITTLE_ENDIAN);	/*56*/
	dest->dir_flag = htoi((unsigned char *) &h->dir_flag, FOREMOST_LITTLE_ENDIAN);					/*60 first sec in the mini fat chain*/
	dest->csectMiniFat = htoi((unsigned char *) &h->csectMiniFat, FOREMOST_LITTLE_ENDIAN);			/*64 number of sectors in the minifat */
	dest->FAT_next_block = htoi((unsigned char *) &h->FAT_next_block, FOREMOST_LITTLE_ENDIAN);		/*68*/
	dest->num_extra_FAT_blocks = htoi((unsigned char *) &h->num_extra_FAT_blocks,
									  FOREMOST_LITTLE_ENDIAN);

	x = (int *) &h[1];
	y = (int *) &dest[1];
	for (i = 0; i < 109; i++, x++)
		{
		*y = htoi((unsigned char *)x, FOREMOST_LITTLE_ENDIAN);
		y++;
		}

	return dest;
}

void dump_ole_header(struct OLE_HDR *h)
{
	int i, *x;

	//fprintf (stderr, "clsid  = ");
	//printx(h->clsid,0,16);
	fprintf(stderr,
			"\nuMinorVersion  = %u\t",
			htos((unsigned char *) &h->uMinorVersion, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uDllVersion  = %u\t",
			htos((unsigned char *) &h->uDllVersion, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uByteOrder  = %u\n",
			htos((unsigned char *) &h->uByteOrder, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uSectorShift  = %u\t",
			htos((unsigned char *) &h->uSectorShift, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"uMiniSectorShift  = %u\t",
			htos((unsigned char *) &h->uMiniSectorShift, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved  = %u\n",
			htos((unsigned char *) &h->reserved, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved1  = %u\t",
			htoi((unsigned char *) &h->reserved1, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"reserved2  = %u\t",
			htoi((unsigned char *) &h->reserved2, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"csectMiniFat = %u\t",
			htoi((unsigned char *) &h->csectMiniFat, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"miniSectorCutoff = %u\n",
			htoi((unsigned char *) &h->miniSectorCutoff, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"root_start_block  = %u\n",
			htoi((unsigned char *) &h->root_start_block, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"dir flag = %u\n",
			htoi((unsigned char *) &h->dir_flag, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"# FAT blocks = %u\n",
			htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"FAT_next_block = %u\n",
			htoi((unsigned char *) &h->FAT_next_block, FOREMOST_LITTLE_ENDIAN));
	fprintf(stderr,
			"# extra FAT blocks = %u\n",
			htoi((unsigned char *) &h->num_extra_FAT_blocks, FOREMOST_LITTLE_ENDIAN));
	x = (int *) &h[1];
	fprintf(stderr, "bbd list:");
	for (i = 0; i < 109; i++, x++)
		{
		if ((i % 10) == 0)
			fprintf(stderr, "\n");
		if (*x == '\xff')
			break;
		fprintf(stderr, "%x ", htoi((unsigned char *)x, FOREMOST_LITTLE_ENDIAN));
		}

	fprintf(stderr, "\n	**************End of header***********\n");
}

int dump_dirent(int which_one)
{
	int				i;
	char			*p;
	short			unknown;
	struct OLE_DIR	*dir;

	dir = (struct OLE_DIR *) &buffer[which_one * sizeof(struct OLE_DIR)];
	if (dir->type == NO_ENTRY)
		return TRUE;
	fprintf(stderr, "DIRENT_%d :\t", dir_count);
	fprintf(stderr,
			"%s\t",
			(dir->type == ROOT) ? "root directory" : (dir->type == STORAGE) ? "directory" : "file");

	/* get UNICODE name */
	p = dir->name;
	if (*p < ' ')
		{
		unknown = *((short *)p);

		//fprintf (stderr, "%04x\t", unknown);
		p += 2; /* step over unknown short */
		}

	for (i = 0; i < dir->namsiz; i++, p++)
		{
		if (*p && (*p > 0x1f))
			{
			if (isprint(*p))
				{
				fprintf(stderr, "%c", *p);
				}
			else
				{
				printf("***	Invalid char %x ***\n", *p);
				return FALSE;
				}
			}
		}

	fprintf(stderr, "\n");

	//fprintf (stderr, "prev_dirent = %lu\t", dir->prev_dirent);
	//fprintf (stderr, "next_dirent = %lu\t", dir->next_dirent);
	//fprintf (stderr, "dir_dirent  = %lu\n", dir->dir_dirent);
	//fprintf (stderr, "name  = %s\t", dir->name);
	fprintf(stderr, "namsiz  = %u\t", dir->namsiz);
	fprintf(stderr, "type  = %d\t", dir->type);
	fprintf(stderr, "reserved  = %u\n", dir->reserved);

	fprintf(stderr, "start block  = %lu\n", dir->start_block);
	fprintf(stderr, "size  = %lu\n", dir->size);
	fprintf(stderr, "\n	**************End of dirent***********\n");
	return TRUE;
}
