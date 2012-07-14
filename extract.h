/*
	local file header signature     4 bytes  (0x04034b50)
        version needed to extract       2 bytes
        general purpose bit flag        2 bytes
        compression method              2 bytes
        last mod file time              2 bytes
        last mod file date              2 bytes
        crc-32                          4 bytes
        compressed size                 4 bytes
        uncompressed size               4 bytes
        filename length                 2 bytes
        extra field length              2 bytes
*/

/*
 	central file header signature   4 bytes  (0x02014b50)
        version made by                 2 bytes
        version needed to extract       2 bytes
        general purpose bit flag        2 bytes
        compression method              2 bytes
        last mod file time              2 bytes
        last mod file date              2 bytes
        crc-32                          4 bytes
        compressed size                 4 bytes
        uncompressed size               4 bytes
        filename length                 2 bytes
        extra field length              2 bytes
        file comment length             2 bytes
        disk number start               2 bytes
        internal file attributes        2 bytes
        external file attributes        4 bytes
        relative offset of local header 4 bytes
*/

/* end of central dir signature    4 bytes  (0x06054b50)
        number of this disk             2 bytes
        number of the disk with the
        start of the central directory  2 bytes
        total number of entries in
        the central dir on this disk    2 bytes
        total number of entries in
        the central dir                 2 bytes
        size of the central directory   4 bytes
        offset of start of central
        directory with respect to
        the starting disk number        4 bytes
        zipfile comment length          2 bytes
        zipfile comment (variable size)
	*/
struct zipLocalFileHeader
{
	unsigned int	signature;					//0
	unsigned short	version;					//4
	unsigned short	genFlag;					//6
	signed short	compression;				//8
	unsigned short	last_mod_time;				//10
	unsigned short	last_mod_date;				//12
	unsigned int	crc;						//14
	unsigned int	compressed;					//18
	unsigned int	uncompressed;				//22
	unsigned short	filename_length;			//26
	unsigned short	extra_length;				//28
};
struct zipCentralFileHeader
{
	unsigned int	signature;					//0
	unsigned char	version_extract[2];			//4
	unsigned char	version_madeby[2];			//6
	unsigned short	genFlag;					//8
	unsigned short	compression;				//10
	unsigned short	last_mod_time;				//12
	unsigned short	last_mod_date;				//14
	unsigned int	crc;						//16
	unsigned int	compressed;					//20
	unsigned int	uncompressed;				//24
	unsigned short	filename_length;			//28
	unsigned short	extra_length;				//30
	unsigned short	filecomment_length;			//32
	unsigned short	disk_number_start;			//34
};
struct zipEndCentralFileHeader
{
	unsigned int	signature;					//0
	unsigned short	numOfdisk;					//4
	unsigned short	compression;				//6
	unsigned short	start_of_central_dir;		//8
	unsigned short	num_entries_in_central_dir; //10
	unsigned int	size_of_central_dir;		//12
	unsigned int	offset;						//16
	unsigned short	comment_length;				//20
};

void print_zip(struct zipLocalFileHeader *fileHeader, struct zipCentralFileHeader *centralHeader)
{
	printf("\n	Local Header Data\n");
	printf("GenFlag:=%d,compressed:=%d,uncompressed:=%d\n",
		   fileHeader->genFlag,
		   fileHeader->compressed,
		   fileHeader->uncompressed);
	printf("Compression:=%d, filename_len:=%d,extralen:=%d\n",
		   fileHeader->compression,
		   fileHeader->filename_length,
		   fileHeader->extra_length);

	printf("	Central Header Data\n");
	printf("GenFlag:=%d,compressed:=%d,uncompressed:=%d\n",
		   centralHeader->genFlag,
		   centralHeader->compressed,
		   centralHeader->uncompressed);
	printf("Compression:=%d, Version Madeby:=%x%x\n",
		   centralHeader->compression,
		   centralHeader->version_madeby[0],
		   centralHeader->version_madeby[1]);
}
