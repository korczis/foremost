#define TRUE			1
#define FALSE			0
#define SPECIAL_BLOCK	- 3
#define END_OF_CHAIN	- 2
#define UNUSED			- 1

#define NO_ENTRY		0
#define STORAGE			1
#define STREAM			2
#define ROOT			5
#define SHORT_BLOCK		3

#define FAT_START		0x4c
#define OUR_BLK_SIZE	512
#define DIRS_PER_BLK	4
#ifndef __CYGWIN
	#define MIN(x, y)	((x) < (y) ? (x) : (y))
#endif

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

struct OLE_HDR
{
	char			magic[8];				/*0*/
	char			clsid[16];				/*8*/
       __U16_TYPE      uMinorVersion;                  /*24*/
       __U16_TYPE      uDllVersion;                    /*26*/
       __U16_TYPE      uByteOrder;                             /*28*/
       __U16_TYPE      uSectorShift;                   /*30*/
       __U16_TYPE      uMiniSectorShift;               /*32*/
       __U16_TYPE      reserved;                               /*34*/
       u_int32_t       reserved1;                              /*36*/
       u_int32_t       reserved2;                              /*40*/
       u_int32_t       num_FAT_blocks;                 /*44*/
       u_int32_t       root_start_block;               /*48*/
       u_int32_t       dfsignature;                    /*52*/
       u_int32_t       miniSectorCutoff;               /*56*/
       u_int32_t       dir_flag;                               /*60 first sec in the mini fat chain*/
       u_int32_t       csectMiniFat;                   /*64 number of sectors in the minifat */
       u_int32_t       FAT_next_block;                 /*68*/
       u_int32_t       num_extra_FAT_blocks;   /*72*/
	/* FAT block list starts here !! first 109 entries  */
};

struct OLE_DIR
{
	char			name[64];
	unsigned short	namsiz;
	char			type;
	char			bflags;					//0 or 1
	unsigned long	prev_dirent;
	unsigned long	next_dirent;
	unsigned long	dir_dirent;
	char			clsid[16];
	unsigned long	userFlags;
	int				secs1;
	int				days1;
	int				secs2;
	int				days2;
	unsigned long	start_block;			//starting SECT of stream
	unsigned long	size;
	short			reserved;				//must be 0
};

struct DIRECTORY
{
	char	name[64];
	int		type;
	int		level;
	int		start_block;
	int		size;
	int		next;
	int		prev;
	int		dir;
	int		s1;
	int		s2;
	int		d1;
	int		d2;
}
*dirlist, *dl;

int				get_dir_block(unsigned char *fd, int blknum, int buffersize);
int				get_dir_info(unsigned char *src);
void			extract_stream(char *fd, int blknum, int size);
void			dump_header(struct OLE_HDR *h);
int				dump_dirent(int which_one);
int				get_block(unsigned char *fd, int blknum, unsigned char *dest, long long int buffersize);
int				get_FAT_block(unsigned char *fd, int blknum, int *dest, int buffersize);
int				reorder_dirlist(struct DIRECTORY *dir, int level);

unsigned char	*get_ole_block(unsigned char *fd, int blknum, unsigned long long buffersize);
struct OLE_HDR	*reverseBlock(struct OLE_HDR *dest, struct OLE_HDR *h);

void			dump_ole_header(struct OLE_HDR *h);
void			*Malloc(size_t bytes);
void			die(char *fmt, void *arg);
void			init_ole();
