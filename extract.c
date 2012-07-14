
	 /* extract.c
 * Copyright (c) 2005, Nick Mikus
 * This file contains the file specific functions used to extract
 * data from an image.
 *
 * Each has a similar structure
 * f_state *s:  state of the program.
 * c_offset:	offset that the header was recorded within the current chunk
 * foundat:	The location the header was "foundat"
 * buflen:	How much buffer is left until the end of the current chunk
 * needle:	Search specification
 * f_offset:	Offset that the current chunk is located within the file
 */

#include "main.h"
#include "extract.h"
#include "ole.h"
extern unsigned char buffer[OUR_BLK_SIZE];
extern int	verbose;
extern int	dir_count;
extern int	block_list[OUR_BLK_SIZE / sizeof(int)];
extern int	*FAT;
extern char *extract_name;
extern int	extract;
extern int	FATblk;
extern int	highblk;

/********************************************************************************
 *Function: extract_zip
 *Description: Given that we have a ZIP header jump through the file headers
    until we reach the EOF.
 *Return: A pointer to where the EOF of the ZIP is in the current buffer
**********************************************************************************/
unsigned char *extract_zip(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset, char *type)
{
	unsigned char				*currentpos = NULL;
	unsigned char				*buf = foundat;
	unsigned short				comment_length = 0;
	unsigned char				*extractbuf = NULL;
	struct zipLocalFileHeader	localFH;
	u_int64_t					bytes_to_search = 50 * KILOBYTE;
	u_int64_t					file_size = 0;
	int							oOffice = FALSE;
	int							office2007 = FALSE;

	char						comment[32];
	localFH.genFlag=0;
	localFH.compressed=0;
	localFH.uncompressed =0;
	if (buflen < 100)
		return NULL;

	if (strncmp((char *) &foundat[30], "mimetypeapplication/vnd.sun.xml.", 32) == 0)
		{
		oOffice = TRUE;
		if (strncmp((char *) &foundat[62], "calc", 4) == 0)
			{
			needle->suffix = "sxc";
			}
		else if (strncmp((char *) &foundat[62], "impress", 7) == 0)
			{
			needle->suffix = "sxi";
			}
		else if (strncmp((char *) &foundat[62], "writer", 6) == 0)
			{
			needle->suffix = "sxw";
			}
		else
			{
			sprintf(comment, " (OpenOffice Doc?)");
			strcat(needle->comment, comment);
			needle->suffix = "sx";
			}
		}
	else
		{
		needle->suffix = "zip";
		}

	
	while (1)	//Jump through each local file header until the central directory structure is reached, much faster than searching 
		{
		
		if (foundat[2] == '\x03' && foundat[3] == '\x04')	//Verfiy we are looking at a local file header//
			{
			
			localFH.compression=htos(&foundat[8], FOREMOST_LITTLE_ENDIAN);
			localFH.compressed = htoi(&foundat[18], FOREMOST_LITTLE_ENDIAN);
			localFH.uncompressed = htoi(&foundat[22], FOREMOST_LITTLE_ENDIAN);
			localFH.filename_length = htos(&foundat[26], FOREMOST_LITTLE_ENDIAN);
			localFH.extra_length = htos(&foundat[28], FOREMOST_LITTLE_ENDIAN);;
			localFH.genFlag = htos(&foundat[6], FOREMOST_LITTLE_ENDIAN);	

			// Sanity checking
			if (localFH.compressed > needle->max_len)
				return foundat + needle->header_len;

			if (localFH.filename_length > 100)
				return foundat + needle->header_len;

			//Check if we should grab more from the disk
			if (localFH.compressed + 30 > buflen - (foundat - buf))
				{
				return NULL;								
				}
				
			//Size of the local file header data structure
			foundat += 30;									

			if (strcmp(needle->suffix,"zip")==0)
				{
				if (strncmp((char *)foundat, "content.xml", 11) == 0 && strcmp(needle->suffix,"zip")==0)
					{
					oOffice = TRUE;
					sprintf(comment, " (OpenOffice Doc?)");
					strcat(needle->comment, comment);
					needle->suffix = "sx";
					}
				else if (strstr((char *)foundat, ".class") || strstr((char *)foundat, ".jar") ||
						 strstr((char *)foundat, ".java"))
					{
					needle->suffix = "jar";
					}
				else if(strncmp((char *)foundat, "[Content_Types].xml",19)==0)
					{
						office2007=TRUE;
					}
				else if(strncmp((char *)foundat, "ppt/slides",10)==0 && office2007==TRUE)
					{
						needle->suffix = "pptx";
					}
				else if(strncmp((char *)foundat, "word/document.xml",17)==0 && office2007==TRUE)
					{	
						needle->suffix = "docx";
					}
				else if(strncmp((char *)foundat, "xl/workbook.xml",15)==0 && office2007==TRUE)
					{	
						needle->suffix = "xlsx";
					}
					
					
				else
					{
						printf("foundat=%s\n",foundat);
					}	
				}

			foundat += localFH.compressed;
			foundat += localFH.filename_length;
			foundat += localFH.extra_length;
			
			if (localFH.genFlag == 8)
				{
#ifdef DEBUG	
					fprintf(stderr,"We have extra stuff!!!");
#endif
				}
			
			
			if(localFH.genFlag & 1<<3 && localFH.uncompressed==0 &&  localFH.compressed==0 )
				{
#ifdef DEBUG
				fprintf(stderr,"No data to jmp Just search for the next file Footer (localFH.genFlag:=%d)\n",localFH.genFlag);
#endif
				break;
				}

	#ifdef DEBUG
				printf("localFH.compressed:=%d  localFH.uncompressed:=%d\n\t jumping %d bytes filename=%d bytes",
					   localFH.compressed,
					   localFH.uncompressed,localFH.filename_length+localFH.compressed+localFH.extra_length,localFH.filename_length);
				printx(foundat, 0, 16);
	#endif

			}	
		else if (oOffice && localFH.genFlag == 8)
			{
			break;
			}
		else
			{
			break;
			}
			
		
	}//end while loop
	
	if (oOffice)
		{

		//We have an OO doc how long should we search for?
		bytes_to_search = 1 * MEGABYTE;
		}
	else if (localFH.genFlag & 1<<3 && localFH.uncompressed==0 &&  localFH.compressed==0 )
		{
		bytes_to_search = needle->max_len;
		}
	else
		{
		bytes_to_search = (buflen < (foundat - buf) ? buflen : buflen - (foundat - buf));
		}

	//Make sure we are not searching more than what he have
        if (buflen <= (foundat - buf)) {
#ifdef DEBUG
		printf("avoided bug in extract_zip!\n");
#endif
		bytes_to_search = 0;
	} else {
		if (buflen - (foundat - buf) < bytes_to_search)
		{
		bytes_to_search = buflen - (foundat - buf);
		}
	}


	currentpos = foundat;
#ifdef DEBUG
	printf("Search for the footer bytes_to_search:=%lld buflen:=%lld\n", bytes_to_search, buflen);
#endif

	foundat = bm_search(needle->footer,
						needle->footer_len,
						foundat,
						bytes_to_search,
						needle->footer_bm_table,
						needle->case_sen,
						SEARCHTYPE_FORWARD);
#ifdef DEBUG
	printf("Search complete \n");
#endif

	if (foundat)											/*Found the end of the central directory structure, determine the exact length and extract*/
	{

		/*Jump to the comment length field*/
#ifdef DEBUG
		printf("distance searched:=%lu\n", foundat - currentpos);
#endif
		if (buflen - (foundat - buf) > 20)
			{
			foundat += 20;
			}
		else
			{
			return NULL;
			}

		comment_length = htos(foundat, FOREMOST_LITTLE_ENDIAN);
		foundat += comment_length + 2;
		file_size = (foundat - buf);
#ifdef DEBUG
		printf("File size %lld\n", file_size);
		printf("Found a %s type:=%s\n", needle->suffix, type);
#endif
		extractbuf = buf;
		if (strcmp(type,"all")==0 || strcmp(type,needle->suffix)==0)
		{
#ifdef DEBUG
			printf("Writing a %s to disk\n", needle->suffix);
#endif
			write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		}

#ifdef DEBUG
		printf("Found a %s\n", needle->suffix);
#endif
		return foundat-2;
	}

	if (bytes_to_search > buflen - (currentpos - buf))
		return NULL;

#ifdef DEBUG
	printf("I give up \n");
#endif
	return currentpos;
}

/********************************************************************************
 *Function: extract_pdf
 *Description: Given that we have a PDF header check if it is Linearized, if so
    grab the file size and we are done, else search for the %%EOF
*Return: A pointer to where the EOF of the PDF is in the current buffer
**********************************************************************************/
unsigned char *extract_pdf(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char		*currentpos = NULL;
	unsigned char		*buf = foundat;
	unsigned char		*extractbuf = NULL;
	unsigned char		*tempsize;
	unsigned long int	size = 0;
	int					file_size = 0;
	unsigned char		*header = foundat;
	int					bytes_to_search = 0;
	char				comment[32];

	foundat += needle->header_len;	/* Jump Past the %PDF HEADER */
	currentpos = foundat;

#ifdef DEBUG
	printf("PDF SEARCH\n");
#endif

	/*Determine when we have searched enough*/
	if (buflen >= needle->max_len)
		{
		bytes_to_search = needle->max_len;
		}
	else
		{
		bytes_to_search = buflen;
		}

	/*Check if the buffer is less than 100 bytes, if so search what we have*/
	if (buflen < 512)
		return NULL;
	else
		{
		currentpos = foundat;

		/*Check for .obj in the first 100 bytes*/
		foundat = bm_search(needle->markerlist[1].value,
							needle->markerlist[1].len,
							foundat,
							100,
							needle->markerlist[1].marker_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);

		if (!foundat)
		{
#ifdef DEBUG
			printf("no obj found\n");
#endif
			return currentpos + 100;
		}

		foundat = currentpos;

		/*Search for "./L " to see if the file is linearized*/
		foundat = bm_search(needle->markerlist[2].value,
							needle->markerlist[2].len,
							foundat,
							512,
							needle->markerlist[2].marker_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);

		if (foundat)
			{
			foundat = bm_search(needle->markerlist[0].value,
								needle->markerlist[0].len,
								foundat,
								512,
								needle->markerlist[0].marker_bm_table,
								needle->case_sen,
								SEARCHTYPE_FORWARD);
			}
		else
		{
#ifdef DEBUG
			printf("not linearized\n");
#endif
		}
		}

	if (foundat)					/*The PDF is linearized extract the size and we are done*/
		{
		sprintf(comment, " (PDF is Linearized)");
		strcat(needle->comment, comment);

		foundat += needle->markerlist[0].len;
		tempsize = (unsigned char *)malloc(8 * sizeof(char));
		tempsize = memcpy(tempsize, foundat, 8);
		size = atoi((char *)tempsize);

		free(tempsize);
		if (size <= 0)
			return foundat;
		if (size > buflen)
			{
			if (size > needle->max_len)
				return foundat;
			else
				return NULL;
			}

		header += size;
		foundat = header;
		foundat -= needle->footer_len;

		/*Jump back 10 bytes and see if we actually have and EOF there*/
		foundat -= 10;
		currentpos = foundat;
		foundat = bm_search(needle->footer,
							needle->footer_len,
							foundat,
							needle->footer_len + 9,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);
		if (foundat)				/*There is an valid EOF at the end, Write to disk*/
			{
			foundat += needle->footer_len + 1;
			file_size = (foundat - buf);

			extractbuf = buf;
			write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);

			return foundat;
			}

		return NULL;

		}
	else							/*Search for Linearized PDF failed, just look for %%EOF */
	{
#ifdef DEBUG
		printf("	Linearized search failed, searching %d bytes, buflen:=%lld\n",
			   bytes_to_search,
			   buflen - (header - buf));
#endif
		foundat = currentpos;
		foundat = bm_search(needle->footer,
							needle->footer_len,
							foundat,
							bytes_to_search,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);

		if (foundat)				/*Write the non-linearized PDF to disk*/
			{
			foundat += needle->footer_len + 1;
			file_size = (foundat - buf);
			extractbuf = buf;

			write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);

			return foundat;

			}

		return NULL;
	}

}

/********************************************************************************
 *Function: extract_cpp
 *Description: Use keywords to attempt to find C/C++ source code
*Return: A pointer to where the EOF of the CPP file is in the current buffer
**********************************************************************************/
unsigned char *extract_cpp(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{

	unsigned char	*header = foundat;
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	int				end = 0;
	int				start = 0;
	int				i = 0;
	int				marker_score = 0;
	int				ok = FALSE;
	int				file_size = 0;
	unsigned char	*footer = NULL;

	/*Search for a " or a < within 20 bytes of a #include statement*/
	for (i = 0; i < 20; i++)
		{
		if (foundat[i] == '\x22' || foundat[i] == '\x3C')
			{
			ok = TRUE;
			}
		}

	if (!ok)
		return foundat + needle->header_len;

	/*Keep running through the buffer until an non printable character is reached*/
	while (isprint(foundat[end]) || foundat[end] == '\x0a' || foundat[end] == '\x09')
		{
		end++;
		}

	foundat += end - 1;
	footer = foundat;

	if (end < 50)
		return foundat;

	/*Now lets go the other way and grab all those comments at the begining of the file*/
	while (isprint(buf[start]) || buf[start] == '\x0a' || buf[start] == '\x09')
		{
		start--;
		}

	header = &buf[start + 1];
	file_size = (footer - header);

	foundat = header;

	/*Now we have an ascii file to look for keywords in*/
	foundat = bm_search(needle->footer,
						needle->footer_len,
						header,
						file_size,
						needle->footer_bm_table,
						FALSE,
						SEARCHTYPE_FORWARD);
	if (foundat)
		marker_score += 1;

	foundat = header;
	foundat = bm_search(needle->markerlist[0].value,
						needle->markerlist[0].len,
						header,
						file_size,
						needle->markerlist[0].marker_bm_table,
						1,
						SEARCHTYPE_FORWARD);
	if (foundat)
		marker_score += 1;

	if (marker_score == 0)
		return foundat;

	if (foundat)
		{
		extractbuf = buf;
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset + start + 1);
		
		return footer;

		}

	return NULL;
}

/********************************************************************************
 *Function: extract_htm
 *Description: Given that we have a HTM header
    search for the file EOF and check that the bytes areound the header are ascii
*Return: A pointer to where the EOF of the HTM is in the current buffer
**********************************************************************************/
unsigned char *extract_htm(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	unsigned char	*currentpos = NULL;

	int				bytes_to_search = 0;
	int				i = 0;
	int				file_size = 0;

	/*Jump past the <HTML tag*/
	foundat += needle->header_len;

	/*Check the first 16 bytes to see if they are ASCII*/
	for (i = 0; i < 16; i++)
		{
		if (!isprint(foundat[i]) && foundat[i] != '\x0a' && foundat[i] != '\x09')
			{
			return foundat + 16;
			}
		}

	/*Determine if the buffer is large enough to encompass a reasonable search*/
	if (buflen < needle->max_len)
		{
		bytes_to_search = buflen - (foundat - buf);
		}
	else
		{
		bytes_to_search = needle->max_len;
		}

	/*Store the current position and search for the HTML> tag*/
	currentpos = foundat;
	foundat = bm_search(needle->footer,
						needle->footer_len,
						foundat,
						bytes_to_search,
						needle->footer_bm_table,
						needle->case_sen,
						SEARCHTYPE_FORWARD);
	if (foundat)	//Found the footer, write to disk
		{
		file_size = (foundat - buf) + needle->footer_len;
		extractbuf = buf;
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		foundat += needle->footer_len;
		return foundat;

		}
	else
		{
		return NULL;
		}

}

/********************************************************************************
 *Function: validOLEheader
 *Description: run various tests aginst an OLE-HEADER to determine whether or not
 	it is valid.
*Return: TRUE/FALSE
**********************************************************************************/
int valid_ole_header(struct OLE_HDR *h)
{

	if (htos((unsigned char *) &h->reserved, FOREMOST_LITTLE_ENDIAN) != 0 ||
		htoi((unsigned char *) &h->reserved1, FOREMOST_LITTLE_ENDIAN) != 0 ||
		htoi((unsigned char *) &h->reserved2, FOREMOST_LITTLE_ENDIAN) != 0)
		{
		return FALSE;
		}

	/*The minimum sector shift is usually 2^6(64) and the uSectorShift is 2^9(512))*/
	if (htos((unsigned char *) &h->uMiniSectorShift, FOREMOST_LITTLE_ENDIAN) != 6 ||
		htos((unsigned char *) &h->uSectorShift, FOREMOST_LITTLE_ENDIAN) != 9 ||
		htoi((unsigned char *) &h->dir_flag, FOREMOST_LITTLE_ENDIAN) < 0)
		{
		return FALSE;
		}

	/*Sanity Checking*/
	if (htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN) <= 0 ||
		htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN) > 100)
		{
		return FALSE;
		}

	if (htoi((unsigned char *) &h->num_extra_FAT_blocks, FOREMOST_LITTLE_ENDIAN) < 0 ||
		htoi((unsigned char *) &h->num_extra_FAT_blocks, FOREMOST_LITTLE_ENDIAN) > 100)
		{
		return FALSE;
		}

	return TRUE;

}

/********************************************************************************
 *Function:checkOleName
 *Description: Determine what type of file is stored in the OLE format based on the
 	names of DIRENT in the FAT table.
*Return: A char* consisting of the suffix of the appropriate file.
**********************************************************************************/
char *check_ole_name(char *name)
{
	if (strstr(name, "WordDocument"))
		{
		return "doc";
		}
	else if (strstr(name, "Worksheet") || strstr(name, "Book") || strstr(name, "Workbook"))
		{
		return "xls";
		}
	else if (strstr(name, "Power"))
		{
		return "ppt";
		}
	else if (strstr(name, "Access") || strstr(name, "AccessObjSiteData"))
		{
		return "mbd";
		}
	else if (strstr(name, "Visio"))
		{
		return "vis";
		}
	else if (strstr(name, "Sfx"))
		{
		return "sdw";
		}
	else
		{
		return NULL;
		}

	return NULL;

}

int adjust_bs(int size, int bs)
{
	int rem = (size % bs);

	if (rem == 0)
		{

		return size;
		}

#ifdef DEBUG
	printf("\tnew size:=%d\n", size + (bs - rem));
#endif
	return (size + (bs - rem));

}

/********************************************************************************
 *Function: extract_ole
 *Description: Given that we have a OLE header, jump through the OLE structure and
    determine what type of file it is.
*Return: A pointer to where the EOF of the OLE is in the current buffer
**********************************************************************************/
unsigned char *extract_ole(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset, char *type)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	char			*temp = NULL;
	char			*suffix = "ole";
	int				totalsize = 0;
	int				extrasize = 0;
	int				oldblk = 0;
	int				i, j;
	int				size = 0;
	int				blknum = 0;
	int				validblk = 512;
	int				file_size = 0;
	int				num_extra_FAT_blocks = 0;
	unsigned char	*htoi_c = NULL;
	int				extra_dir_blocks = 0;
	int				num_FAT_blocks = 0;
	int				next_FAT_block = 0;
	unsigned char	*p;
	int				fib = 1024;
	struct OLE_HDR	*h = NULL;

	int				result = 0;
	int				highblock = 0;
	unsigned long	miniSectorCutoff = 0;
	unsigned long	csectMiniFat = 0;

	/*Deal with globals defined in the OLE API, ugly*/
	if (dirlist != NULL)
		free(dirlist);
	if (FAT != NULL)
		free(FAT);
	init_ole();

	if (buflen < validblk)
		validblk = buflen;
	h = (struct OLE_HDR *)foundat;	/*cast the header block to point at foundat*/
#ifdef DEBUG
	dump_header(h);
#endif
	num_FAT_blocks = htoi((unsigned char *) &h->num_FAT_blocks, FOREMOST_LITTLE_ENDIAN);

	if (!valid_ole_header(h))
		return (buf + validblk);

	miniSectorCutoff = htoi((unsigned char *) &h->miniSectorCutoff, FOREMOST_LITTLE_ENDIAN);
	csectMiniFat = htoi((unsigned char *) &h->csectMiniFat, FOREMOST_LITTLE_ENDIAN);
	next_FAT_block = htoi((unsigned char *) &h->FAT_next_block, FOREMOST_LITTLE_ENDIAN);
	num_extra_FAT_blocks = htoi((unsigned char *) &h->num_extra_FAT_blocks, FOREMOST_LITTLE_ENDIAN);

	FAT = (int *)Malloc(OUR_BLK_SIZE * (num_FAT_blocks + 1));
	p = (unsigned char *)FAT;
	memcpy(p, &h[1], OUR_BLK_SIZE - FAT_START);
	if (next_FAT_block > 0)
		{
		p += (OUR_BLK_SIZE - FAT_START);
		blknum = next_FAT_block;
		for (i = 0; i < num_extra_FAT_blocks; i++)
			{
			if (!get_block(buf, blknum, p, buflen))
				return buf + validblk;
			validblk = (blknum + 1) * OUR_BLK_SIZE;
			p += OUR_BLK_SIZE - sizeof(int);
			blknum = htoi(p, FOREMOST_LITTLE_ENDIAN);
			}
		}

	blknum = htoi((unsigned char *) &h->root_start_block, FOREMOST_LITTLE_ENDIAN);

	if(blknum < 0)
	{
		return buf + 10;
	}

	highblock = htoi((unsigned char *) &h->dir_flag, FOREMOST_LITTLE_ENDIAN);
#ifdef DEBUG
	printf("getting dir block\n");
#endif

	//if(!get_dir_block (buf, blknum, buflen)) return buf+validblk;
	if (!get_block(buf, blknum, buffer, buflen))
		return buf + validblk;		/*GET DIR BLOCK*/
#ifdef DEBUG
	printf("done getting dir block\n");
#endif
	validblk = (blknum + 1) * OUR_BLK_SIZE;	
	while (blknum != END_OF_CHAIN)
	{
#ifdef DEBUG
		printf("finding dir info extra_dir_blks:=%d\n", extra_dir_blocks);
#endif
		if (extra_dir_blocks > 300)
			return buf + validblk;

		/**PROBLEMA**/
#ifdef DEBUG
		printf("***blknum:=%d FATblk:=%d ourblksize=%d\n", blknum, FATblk,OUR_BLK_SIZE);
#endif
		oldblk = blknum;
		htoi_c = (unsigned char *) &FAT[blknum / (OUR_BLK_SIZE / sizeof(int))];

		FATblk = htoi(htoi_c, FOREMOST_LITTLE_ENDIAN);
#ifdef DEBUG
		printf("***blknum:=%d FATblk:=%d\n", blknum, FATblk);
#endif

		if (!get_FAT_block(buf, blknum, block_list, buflen))
			return buf + validblk;
		blknum = htoi((unsigned char *) &block_list[blknum % 128], FOREMOST_LITTLE_ENDIAN);
#ifdef DEBUG
		printf("**blknum:=%d FATblk:=%d\n", blknum, FATblk);
#endif
		if (blknum == END_OF_CHAIN || oldblk == blknum)
		{
#ifdef DEBUG
			printf("EOC\n");
#endif
			break;
		}

		extra_dir_blocks++;
		result = get_dir_block(buf, blknum, buflen);
		if (result == SHORT_BLOCK)
		{
#ifdef DEBUG
			printf("SHORT BLK\n");
#endif
			break;
		}
		else if (!result)
			return buf + validblk;

	}

#ifdef DEBUG
	printf("DONE WITH WHILE\n");
#endif
	blknum = htoi((unsigned char *) &h->root_start_block, FOREMOST_LITTLE_ENDIAN);
	size = OUR_BLK_SIZE * (extra_dir_blocks + 1);
	dirlist = (struct DIRECTORY *)Malloc(size);
	memset(dirlist, 0, size);

	if (!get_block(buf, blknum, buffer, buflen))
		return buf + validblk;		/*GET DIR BLOCK*/

	if (!get_dir_info(buffer))
		{
		return foundat + validblk;
		}

	for (i = 0; i < extra_dir_blocks; i++)
		{
		if (!get_FAT_block(buf, blknum, block_list, buflen))
			return buf + validblk;
		blknum = htoi((unsigned char *) &block_list[blknum % 128], FOREMOST_LITTLE_ENDIAN);
		if (blknum == END_OF_CHAIN)
			break;
#ifdef DEBUG
		printf("getting dir blk blknum=%d\n", blknum);
#endif
		if (!get_block(buf, blknum, buffer, buflen))
			return buf + validblk;	/*GET DIR BLOCK*/
		if (!get_dir_info(buffer))
			{
			return buf + validblk;
			}
		}

#ifdef DEBUG
	printf("dir count is %d\n", i);
#endif
	for (dl = dirlist, i = 0; i < dir_count; i++, dl++)
		{
		memset(buffer, ' ', 75);
		j = htoi((unsigned char *) &dl->level, FOREMOST_LITTLE_ENDIAN) * 4;
		sprintf((char *) &buffer[j], "%-s", dl->name);
		j = strlen((char *)buffer);

		if (dl->name[0] == '@')
			return foundat + validblk;
		if (dl->type == STREAM)
			{
			buffer[j] = ' ';
			sprintf((char *) &buffer[60], "%8d\n", dl->size);

			if (temp == NULL)		/*check if we have alread defined the type*/
				{
				temp = check_ole_name(dl->name);
				if (temp)
					suffix = temp;
				}

			if (dl->size > miniSectorCutoff)
				{
				totalsize += adjust_bs(dl->size, 512);
				}
			else
				{
				totalsize += adjust_bs(dl->size, 64);
				}

#ifdef DEBUG
			fprintf(stdout, buffer);
#endif
			}
		else
			{
			sprintf((char *) &buffer[j], "\n");
#ifdef DEBUG
			printf("\tnot stream data \n");
			fprintf(stdout, buffer);
#endif

			extrasize += adjust_bs(dl->size, 512);

			}
		}

	totalsize += fib;
#ifdef DEBUG
	printf("DIR SIZE:=%d, numFATblks:=%d MiniFat:=%d\n",
		   adjust_bs(((dir_count) * 128), 512),
		   (num_FAT_blocks * 512),
		   adjust_bs((64 * csectMiniFat), 512));
#endif
	totalsize += adjust_bs(((dir_count) * 128), 512);
	totalsize += (num_FAT_blocks * 512);
	totalsize += adjust_bs((64 * csectMiniFat), 512);
	if ((highblk + 5) > highblock && highblk > 0)
		{
		highblock = highblk + 5;
		}

	highblock = highblock * 512;

#ifdef DEBUG
	printf("\t highblock:=%d\n", highblock);
#endif
	if (highblock > totalsize)
	{
#ifdef DEBUG
		printf("	Total size:=%d a difference of %lld\n", totalsize, buflen - totalsize);
		printf("	Extra size:=%d \n", extrasize);
		printf("	Highblock is greater than totalsize\n");
#endif
		totalsize = highblock;
	}

	totalsize = adjust_bs(totalsize, 512);
#ifdef DEBUG
	printf("	Total size:=%d a difference of %lld\n", totalsize, buflen - totalsize);
	printf("	Extra size:=%d \n", extrasize);
#endif

	if (buflen < totalsize)
	{
#ifdef DEBUG
		printf("	***Error not enough left in the buffer left:=%lld needed=%d***\n",
			   buflen,
			   totalsize);
#endif
		totalsize = buflen;
	}

	foundat = buf;
	highblock -= 5 * 512;
	if (highblock > 0 && highblock < buflen)
		{
		foundat += highblock;
		}
	else
		{
		foundat += totalsize;
		}

	/*Return to the highest blknum read in the file, that way we don't miss files that are close*/
	file_size = totalsize;
	extractbuf = buf;

	if (suffix)
		needle->suffix = suffix;

	if (!strstr(needle->suffix, type) && strcmp(type,"all")!=0)
		{
		return foundat;
		}

	write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
	return foundat;

}

//********************************************************************************/
int check_mov(unsigned char *atom)
{
#ifdef DEBUG
	printf("Atom:= %c%c%c%c\n", atom[0], atom[1], atom[2], atom[3]);
#endif
	if (strncmp((char *)atom, "free", 4) == 0 || strncmp((char *)atom, "mdat", 4) == 0 ||
		strncmp((char *)atom, "free", 4) == 0 || strncmp((char *)atom, "wide", 4) == 0 ||
		strncmp((char *)atom, "PICT", 4) == 0)
		{
		return TRUE;
		}

	if (strncmp((char *)atom, "trak", 4) == 0 || strncmp((char *)atom, "mdat", 4) == 0 ||
		strncmp((char *)atom, "mp3", 3) == 0 || strncmp((char *)atom, "wide", 4) == 0 ||
		strncmp((char *)atom, "moov", 4) == 0)
		{
		return TRUE;
		}

	return FALSE;
}

/********************************************************************************
 *Function: extract_mov
 *Description: Given that we have a MOV header JUMP through the mov data structures
    until we reach EOF
*Return: A pointer to where the EOF of the MOV is in the current buffer
**********************************************************************************/
unsigned char *extract_mov(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat - 4;
	unsigned char	*extractbuf = NULL;
	unsigned int	atomsize = 0;
	unsigned int	filesize = 0;
	int				mdat = FALSE;
	foundat -= 4;
	buflen += 4;
	while (1)						/*Loop through all the atoms until the EOF is reached*/
		{
		atomsize = htoi(foundat, FOREMOST_BIG_ENDIAN);
#ifdef DEBUG
		printf("Atomsize:=%d\n", atomsize);
#endif
		if (atomsize <= 0 || atomsize > needle->max_len)
			{
			return foundat + needle->header_len + 4;
			}

		filesize += atomsize;		/*Add the atomsize to the total file size*/
		if (filesize > buflen)
		{
#ifdef DEBUG
			printf("file size > buflen fs:=%d bf:=%lld\n", filesize, buflen);
#endif
			if (buflen >= needle->max_len)
				return foundat + needle->header_len + 4;
			else
				{
				return NULL;
				}
		}

		foundat += atomsize;
		if (buflen - (foundat - buf) < 5)
			{
			if (mdat)
				{
				break;
				}
			else
			{
#ifdef DEBUG
				printf("No mdat found");
#endif
				return foundat;
			}
			}

		/*Check if we have an mdat atom, these are required thus can be used to
	* Weed out corrupted file*/
		if (strncmp((char *)foundat + 4, "mdat", 4) == 0)
			{
			mdat = TRUE;
			}

		if (check_mov(foundat + 4)) /*Check to see if we are at a valid header*/
		{
#ifdef DEBUG
			printf("Checkmov succeeded\n");
#endif
		}
		else
		{
#ifdef DEBUG
			printf("Checkmov failed\n");
#endif
			if (mdat)
				{
				break;
				}
			else
			{
#ifdef DEBUG
				printf("No mdat found");
#endif
				return foundat;

			}
		}
		}							//End loop

	if (foundat)
		{

		filesize = (foundat - buf);
#ifdef DEBUG
		printf("file size:=%d\n", filesize);
#endif
		extractbuf = buf;
		write_to_disk(s, needle, filesize, extractbuf, c_offset + f_offset - 4);
		return foundat;
		}

#ifdef DEBUG
	printf("NULL Atomsize:=%d\n", atomsize);
#endif
	return NULL;

}

/********************************************************************************
 *Function: extract_wmv
 *Description: Given that we have a WMV header
    search for the file header and grab the file size.
*Return: A pointer to where the EOF of the WMV is in the current buffer
**********************************************************************************/
unsigned char *extract_wmv(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{

	unsigned char	*currentpos = NULL;
	unsigned char	*header = foundat;
	unsigned char	*extractbuf = NULL;
	unsigned char	*buf = foundat;
	unsigned int		size = 0;
	u_int64_t		file_size = 0;
	u_int64_t			headerSize = 0;
	u_int64_t			fileObjHeaderSize = 0;
	int				numberofHeaderObjects = 0;
	int				reserved[2];
	int				bytes_to_search = 0;

	/*If we have less than a WMV header bail out*/
	if (buflen < 70)
		return NULL;

	foundat += 16;		/*Jump to the header size*/
	headerSize = htoll(foundat, FOREMOST_LITTLE_ENDIAN);
	//printx(foundat,0,8);
	foundat += 8;
	numberofHeaderObjects = htoi(foundat, FOREMOST_LITTLE_ENDIAN);
	foundat += 4;		//Jump to the begin File properties obj
	reserved[0] = foundat[0];
	reserved[1] = foundat[1];
	foundat += 2;
	//printf("found WMV\n");
	//end header obj
	//****************************************************/
	//Sanity Check
	//printf("WMV num_header_objs=%d headerSize=%llu\n",numberofHeaderObjects,headerSize);

	if (headerSize <= 0 || numberofHeaderObjects <= 0 || reserved[0] != 1)
		{
		printf("WMV err num_header_objs=%d headerSize=%llu\n",numberofHeaderObjects,headerSize);
		return foundat;
		}

	currentpos = foundat;
	if (buflen - (foundat - buf) >= needle->max_len)
		bytes_to_search = needle->max_len;
	else
		bytes_to_search = buflen - (foundat - buf);

	/*Note we are not searching for the footer here, just the file header ID so we can get the file size*/
	foundat = bm_search(needle->footer,
						needle->footer_len,
						foundat,
						bytes_to_search,
						needle->footer_bm_table,
						needle->case_sen,
						SEARCHTYPE_FORWARD);
	if (foundat)
		{
		foundat += 16;	/*jump to the headersize*/
		fileObjHeaderSize = htoll(foundat, FOREMOST_LITTLE_ENDIAN);
		//printx(foundat,0,8);
		foundat += 24;	//Jump to the file size obj
		size = htoi(foundat, FOREMOST_LITTLE_ENDIAN);
		//printx(foundat,0,8);
		
#ifdef DEBUG
		printf("SIZE:=%u fileObjHeaderSize=%llu\n", size,fileObjHeaderSize);
#endif
		}
	else
		{
		return NULL;
		}

	/*Sanity check data*/
	if (size > 0 && size <= needle->max_len && size <= buflen)
		{
		header += size;
#ifdef DEBUG
		printf("	Found a WMV at:=%lld,File size:=%lld\n", c_offset, size);
		printf("	Headersize:=%d, numberofHeaderObjects:= %d ,reserved:=%d,%d\n",
			   headerSize,
			   numberofHeaderObjects,
			   reserved[0],
			   reserved[1]);
#endif

		/*Everything seem ok, write to disk*/
		file_size = (header - buf);
		extractbuf = buf;
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		foundat += file_size;
		return header;
		}

	return NULL;

}

/********************************************************************************
 *Function: extract_riff
 *Description: Given that we have a RIFF header parse header and grab the file size.
 *Return: A pointer to where the EOF of the RIFF is in the current buffer
 **********************************************************************************/
unsigned char *extract_riff(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
							s_spec *needle, u_int64_t f_offset, char *type)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	int				size = 0;
	u_int64_t		file_size = 0;

	size = htoi(&foundat[4], FOREMOST_LITTLE_ENDIAN);		/* Grab the total file size in little endian from offset 4*/
	if (strncmp((char *) &foundat[8], "AVI", 3) == 0)		/*Sanity Check*/
		{
		if (strncmp((char *) &foundat[12], "LIST", 4) == 0) /*Sanity Check*/
			{
			if (size > 0 && size <= needle->max_len && size <= buflen)
			{
#ifdef DEBUG
				printf("\n	Found an AVI at:=%lld,File size:=%d\n", c_offset, size);
#endif
				file_size = size;
				extractbuf = buf;
				needle->suffix = "avi";
				if (!strstr(needle->suffix, type) && strcmp(type,"all")!=0)
					return foundat + size;
				write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
				foundat += size;
				return foundat;
			}

			return buf + needle->header_len;

			}
		else
			{
			return buf + needle->header_len;
			}
		}
	else if (strncmp((char *) &foundat[8], "WAVE", 4) == 0) /*Sanity Check*/
		{
		if (size > 0 && size <= needle->max_len && size <= buflen)
		{
#ifdef DEBUG
			printf("\n	Found a WAVE at:=%lld,File size:=%d\n", c_offset, size);
#endif

			file_size = size;
			extractbuf = buf;
			needle->suffix = "wav";
			if (!strstr(needle->suffix, type) && strcmp(type,"all")!=0)
				return foundat + size;

			write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
			foundat += file_size;
			return foundat;
		}

		return buf + needle->header_len;

		}
	else
		{
		return buf + needle->header_len;
		}

	return NULL;

}

/********************************************************************************
 *Function: extract_bmp
 *Description: Given that we have a BMP header parse header and grab the file size.
 *Return: A pointer to where the EOF of the BMP is in the current buffer
 **********************************************************************************/
unsigned char *extract_bmp(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	int				size = 0;
	int				headerlength = 0;
	int				v_size = 0;
	int				h_size = 0;
	unsigned char	*extractbuf = NULL;
	u_int64_t		file_size = 0;
	char			comment[32];
	int				dataOffset = 0;
	int				dataSize = 0;

	if (buflen < 100)
		return buf + needle->header_len;

	/*JUMP the first to bytes of the header (BM)*/
	size = htoi(&foundat[2], FOREMOST_LITTLE_ENDIAN);	/*Grab the total file size in little_endian*/

	/*Sanity Check*/
	if (size <= 100 || size > needle->max_len)
		return buf + needle->header_len;

	dataOffset = htoi(&foundat[10], FOREMOST_LITTLE_ENDIAN);
	dataSize = htoi(&foundat[34], FOREMOST_LITTLE_ENDIAN);

	headerlength = htoi(&foundat[14], FOREMOST_LITTLE_ENDIAN);

	if (dataSize + dataOffset != size)
		{

		//printf("newtest != dataSize:=%d dataOffset:=%d\n",dataSize,dataOffset);
		}

	//Header length
	if (headerlength > 1000 || headerlength <= 0)
		return buf + needle->header_len;

	//foundat+=4;
	v_size = htoi(&foundat[22], FOREMOST_LITTLE_ENDIAN);
	h_size = htoi(&foundat[18], FOREMOST_LITTLE_ENDIAN);

	//Vertical length
	if (v_size <= 0 || v_size > 2000 || h_size <= 0)
		return buf + needle->header_len;

#ifdef DEBUG
	printf("\n	The size of the BMP is %d, Header length:=%d , Vertical Size:= %d, dataSize:=%d dataOffset:=%d\n",
	   size,
		   headerlength,
		   v_size,
		   dataSize,
		   dataOffset);
#endif
	if (size <= buflen)
		{

		sprintf(comment, " (%d x %d)", h_size, v_size);
		strcat(needle->comment, comment);

		file_size = size;
		extractbuf = buf;
		
		write_to_disk(s, needle, file_size, extractbuf, (c_offset + f_offset));
		foundat += file_size;
		return foundat;

		}

	return NULL;
}

/********************************************************************************
 *Function: extract_gif
 *Description: Given that we have a GIF header parse the given buffer to determine
 *	where the file ends.
 *Return: A pointer to where the EOF of the GIF is in the current buffer
 **********************************************************************************/
unsigned char *extract_gif(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*currentpos = foundat;
	unsigned char	*extractbuf = NULL;
	int				bytes_to_search = 0;
	unsigned short	width = 0;
	unsigned short	height = 0;
	u_int64_t		file_size = 0;
	char			comment[32];
	foundat += 4;		/*Jump the first 4 bytes of the gif header (GIF8)*/

	/*Check if the GIF is type 89a or 87a*/
	if (strncmp((char *)foundat, "9a", 2) == 0 || strncmp((char *)foundat, "7a", 2) == 0)
		{
		foundat += 2;	/*Jump the length of the header*/
		width = htos(foundat, FOREMOST_LITTLE_ENDIAN);
		height = htos(&foundat[2], FOREMOST_LITTLE_ENDIAN);

		sprintf(comment, " (%d x %d)", width, height);
		strcat(needle->comment, comment);

		currentpos = foundat;
		if (buflen - (foundat - buf) >= needle->max_len)
			bytes_to_search = needle->max_len;
		else
			bytes_to_search = buflen - (foundat - buf);
		foundat = bm_search(needle->footer,
							needle->footer_len,
							foundat,
							bytes_to_search,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);
		if (foundat)
		{

			/*We found the EOF, write the file to disk and return*/
#ifdef DEBUG
			printx(foundat, 0, 16);
#endif
			file_size = (foundat - buf) + needle->footer_len;
#ifdef DEBUG
			printf("The GIF file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif
			extractbuf = buf;
			write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
			foundat += needle->footer_len;
			return foundat;
		}

		return NULL;

		}
	else				/*Invalid GIF header return the current pointer*/
		{
		return foundat;
		}

}

/********************************************************************************
 *Function: extract_mpg
 * Not done yet
 **********************************************************************************/
unsigned char *extract_mpg(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*currentpos = NULL;

	unsigned char	*extractbuf = NULL;
	int				bytes_to_search = 0;
	unsigned short	size = 0;
	u_int64_t		file_size = 0;

	/*
    size=htos(&foundat[4],FOREMOST_BIG_ENDIAN);
    printf("size:=%d\n",size);

    printx(foundat,0,16);
    foundat+=4;
    */
	int				j = 0;
	if (foundat[15] == (unsigned char)'\xBB')
		{
		}
	else
		{

		return buf + needle->header_len;
		}

	if (buflen <= 2 * KILOBYTE)
		{
		bytes_to_search = buflen;
		}
	else
		{
		bytes_to_search = 2 * KILOBYTE;
		}

	while (1)
		{
		j = 0;
		currentpos = foundat;
#ifdef DEBUG
		printf("Searching for marker\n");
#endif
		foundat = bm_search(needle->markerlist[0].value,
							needle->markerlist[0].len,
							foundat,
							bytes_to_search,
							needle->markerlist[0].marker_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);

		if (foundat)
		{
#ifdef DEBUG
			printf("Found after searching %d\n", foundat - currentpos);
#endif
			while (1)
				{

				if (foundat[3] >= (unsigned char)'\xBB' && foundat[3] <= (unsigned char)'\xEF')
				{
#ifdef DEBUG
					printf("jumping %d:\n", j);
#endif
					size = htos(&foundat[4], FOREMOST_BIG_ENDIAN);
#ifdef DEBUG
					printf("\t hit: ");
					printx(foundat, 0, 16);
					printf("size:=%d\n\tjump: ", size);
#endif
					file_size += (foundat - buf) + size;
					if (size <= 0 || size > buflen - (foundat - buf))
					{
#ifdef DEBUG
						printf("Not enough room in the buffer ");
#endif
						if (size <= 50 * KILOBYTE && size > 0)
							{

							/*We should probably search more*/
							if (file_size < needle->max_len)
								{
								return NULL;
								}
							else
								{
								break;
								}
							}
						else
							{
							return currentpos + needle->header_len;
							}
					}

					foundat += size + 6;
#ifdef DEBUG
					printx(foundat, 0, 16);
#endif
					j++;
				}
				else
					{

					break;
					}
				}

			if (foundat[3] == (unsigned char)'\xB9')
				{
				break;
				}
			else if (foundat[3] != (unsigned char)'\xBA' && foundat[3] != (unsigned char)'\x00')
				{

				/*This is the error state where this doesn't seem to be an mpg anymore*/
				size = htos(&foundat[4], FOREMOST_BIG_ENDIAN);
#ifdef DEBUG
				printf("\t ***TEST: %x\n", foundat[3]);
				printx(foundat, 0, 16);

				printf("size:=%d\n", size);
#endif
				if ((currentpos - buf) >= 1 * MEGABYTE)
					{
					foundat = currentpos;
					break;
					}

				return currentpos + needle->header_len;

				}
			else if (foundat[3] == (unsigned char)'\xB3')
				{
				foundat += 3;
				}
			else
				{
				foundat += 3;
				}
		}
		else
			{
			if ((currentpos - buf) >= 1 * MEGABYTE)
				{
				foundat = currentpos;
				break;
				}
			else
			{
#ifdef DEBUG
				printf("RETURNING BUF\n");
#endif
				return buf + needle->header_len;
			}
			}
		}

	if (foundat)
		{
		file_size = (foundat - buf) + needle->footer_len;
		if (file_size < 1 * KILOBYTE)
			return buf + needle->header_len;
		}
	else
		{
		return buf + needle->header_len;
		}

	if (file_size > buflen)
		file_size = buflen;
	foundat = buf;
#ifdef DEBUG
	printf("The file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif

	extractbuf = buf;
	write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
	foundat += file_size;
	return foundat;
}


/********************************************************************************
 *Function: extract_mp4
 * Not done yet
 **********************************************************************************/
unsigned char *extract_mp4(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;

	unsigned char	*extractbuf = NULL;
	unsigned int	size = 0;
	u_int64_t		file_size = 0;

   
	while(1)
	{
	 	size=htoi(&foundat[28],FOREMOST_BIG_ENDIAN);
		if(size ==0)
		{
			//printf("size ==0\n");
			foundat+=28;
			break;
		}
    		//printf("size:=%d\n",size);
		if(size > 0 && size < buflen)
		{
			if(!isprint(foundat[32]) ||  !isprint(foundat[33]))
			{
				//printf("print err\n");
				break;
				//return foundat+8;
			}
			foundat+=size;
			
		}
		else
		{
			if (size < needle->max_len)
			{
				//printf("Searching More\n");
				return NULL;
			}
			else
			{
				//printf("ERR\n");
				//return foundat+8;
				break;
			}
		}	
	
		//printx(foundat,0,32);

	}
	if (foundat)
	{
		file_size = (foundat - buf) + needle->footer_len;
		if (file_size < 1 * KILOBYTE)
			return buf + needle->header_len;
	}
	

	if (file_size > buflen)
		file_size = buflen;
	foundat = buf;


	extractbuf = buf;	
	write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
	foundat += file_size;
	return foundat;
}


/********************************************************************************
 *Function: extract_png
 *Description: Given that we have a PNG header parse the given buffer to determine
 *	where the file ends.
 *Return: A pointer to where the EOF of the PNG is in the current buffer
 **********************************************************************************/
unsigned char *extract_png(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*currentpos = NULL;

	unsigned char	*extractbuf = NULL;
	int				size = 0;
	int				height = 0;
	int				width = 0;
	u_int64_t		file_size = 0;
	char			comment[32];

	if (buflen < 100)
		return NULL;
	foundat += 8;
	width = htoi(&foundat[8], FOREMOST_BIG_ENDIAN);
	height = htoi(&foundat[12], FOREMOST_BIG_ENDIAN);

	if (width < 1 || height < 1)
		return foundat;

	if (width > 3000 || height > 3000)
		return foundat;

	sprintf(comment, " (%d x %d)", width, height);
	strcat(needle->comment, comment);

	while (1)	/* Jump through the headers until we reach the "data" part of the file*/
		{
		size = htoi(foundat, FOREMOST_BIG_ENDIAN);
#ifdef DEBUG
		printx(foundat, 0, 16);
		printf("Size:=%d\n", size);
#endif

		currentpos = foundat;
		if (size <= 0 || size > buflen - (foundat - buf))
		{
#ifdef DEBUG
			printf("buflen - (foundat-buf)=%lu\n", buflen - (foundat - buf));
#endif
			return currentpos;
		}

		/*12 is the length of the size, TYPE, and CRC field*/
		foundat += size + 12;

		if (isprint(foundat[4]))
			{
			if (strncmp((char *) &foundat[4], "IEND", 4) == 0)
				{
				break;
				}
			}
		else
		{
#ifdef DEBUG
			printx(foundat, 0, 16);
			printf("Not ascii returning\n");
#endif
			return currentpos;
		}

		}

	if (foundat)
		{
		file_size = (foundat - buf) + htoi(foundat, FOREMOST_BIG_ENDIAN) + 12;

		if (file_size > buflen)
			file_size = buflen;
		foundat = buf;
#ifdef DEBUG
		printf("The file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif
		extractbuf = buf;
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		foundat += file_size;
		return foundat;
		}

	return NULL;
}

/********************************************************************************
 *Function: extract_jpeg
 *Description: Given that we have a JPEG header parse the given buffer to determine
 *	where the file ends.
 *Return: A pointer to where the EOF of the JPEG is in the current buffer
 **********************************************************************************/
unsigned char *extract_jpeg(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
							s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*currentpos = NULL;

	unsigned char	*extractbuf = NULL;
	unsigned short	headersize;
	int				bytes_to_search = 0;
	int				hasTable = FALSE;
	int				hasHuffman = FALSE;
	u_int64_t		file_size = 0;

	// char comment[32];

	/*Check if we have a valid header*/
	if (buflen < 128)
		{
		return NULL;
		}

	if (foundat[3] == (unsigned char)'\xe0')
		{

		//JFIF header
		//sprintf(comment," (JFIF)");
		//strcat(needle->comment,comment);
		}
	else if (foundat[3] == (unsigned char)'\xe1')
		{

		//sprintf(comment," (EXIF)");
		//strcat(needle->comment,comment);
		}
	else
		return foundat + needle->header_len;	//Invalid keep searching
	while (1)									/* Jump through the headers until we reach the "data" part of the file*/
	{
#ifdef DEBUG
		printx(foundat, 0, 16);
#endif
		foundat += 2;
		headersize = htos(&foundat[2], FOREMOST_BIG_ENDIAN);
#ifdef DEBUG
		printf("Headersize:=%d buflen:=%lld\n", headersize, buflen);
#endif

		
		if (((foundat + headersize) - buf) > buflen){ return NULL; }	

		foundat += headersize;
		
		if (foundat[2] != (unsigned char)'\xff')
			{
			break;
			}

		/*Ignore 2 "0xff" side by side*/
		if (foundat[2] == (unsigned char)'\xff' && foundat[3] == (unsigned char)'\xff')
			{
			foundat++;
			}

		if (foundat[3] == (unsigned char)'\xdb' || foundat[4] == (unsigned char)'\xdb')
			{
			hasTable = TRUE;
			}
		else if (foundat[3] == (unsigned char)'\xc4')
			{
			hasHuffman = TRUE;
			}
	}

	/*All jpegs must contain a Huffman marker as well as a quantization table*/
	if (!hasTable || !hasHuffman)
	{
#ifdef DEBUG
		printf("No Table or Huffman \n");
#endif
		return buf + needle->header_len;
	}

	currentpos = foundat;

	//sprintf("Searching for footer\n");
	if (buflen < (foundat - buf)) {
#ifdef DEBUG
		printf("avoided bug in extract_jpeg!\n");
#endif
		bytes_to_search = 0;
	} else {
		if (buflen - (foundat - buf) >= needle->max_len)
			bytes_to_search = needle->max_len;
		else
			bytes_to_search = buflen - (foundat - buf);
	}

	foundat = bm_search(needle->footer,
						needle->footer_len,
						foundat,
						bytes_to_search,
						needle->footer_bm_table,
						needle->case_sen,
						SEARCHTYPE_FORWARD);

	if (foundat)								/*Found found a valid JPEG*/
		{

		/*We found the EOF, write the file to disk and return*/
		file_size = (foundat - buf) + needle->footer_len;
#ifdef DEBUG
		printf("The jpeg file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif

		//extractbuf=(unsigned char*) malloc(file_size*sizeof(char));
		//memcpy(extractbuf,buf,file_size);
		extractbuf = buf;
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		foundat += needle->footer_len;

		////free(extractbuf);
		return foundat;
		}
	else
		{
		return NULL;
		}

}	//End extract_jpeg

/********************************************************************************
 *Function: extract_generic
 *Description:
 *Return: A pointer to where the EOF of the
 **********************************************************************************/
unsigned char *extract_generic(f_state *s, u_int64_t c_offset, unsigned char *foundat,
							   u_int64_t buflen, s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*endptr = foundat;
	unsigned char	*beginptr = foundat;
	unsigned char	*extractbuf = NULL;
	int		bytes_to_search = 0;
	u_int64_t	file_size = 0;
	int begin=0;
	int end=0;
	

	if (buflen - (foundat - buf) >= needle->max_len)
		bytes_to_search = needle->max_len;
	else
		bytes_to_search = buflen - (foundat - buf);

  	if(needle->searchtype ==SEARCHTYPE_FORWARD_NEXT)
	{
			foundat+=needle->header_len;
			foundat = bm_search(needle->header,
							needle->header_len,
							foundat,
							bytes_to_search,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);
	}
	else if(needle->searchtype ==SEARCHTYPE_ASCII)
	{
			
	
			while (isprint(foundat[end]) || foundat[end] == '\x0a' || foundat[end] == '\x0d' || foundat[end] == '\x09')
			{
				end++;
			}
			
			foundat+=end;
			endptr=foundat;
			foundat=buf;
			
			while (isprint(foundat[begin-1]) || foundat[begin-1] == '\x0a' || foundat[begin-1] == '\x0d' || foundat[begin-1] == '\x09')
			{
				begin--;
			}
			
			foundat+=begin;
			beginptr=foundat;
			
			buf=beginptr;
			foundat=endptr;
			//printx(buf,0,4);	
			
			file_size=end-begin;	
			//fprintf(stderr,"file_size=%llu end=%d begin=%d ptrsize=%d ptrsize2=%d\n",file_size,end,begin,endptr-beginptr,foundat-buf);
			if(buf==foundat) 
			{
					fprintf(stderr,"Returning Foundat\n");
					return foundat+needle->header_len;
			}			
	}
  	else if (needle->footer == NULL || strlen((char *)needle->footer) < 1)
	{
#ifdef DEBUG
		printf("footer is NULL\n");
#endif
		foundat = NULL;
	}
	else
	{
#ifdef DEBUG
		printf("footer is not NULL %p\n", needle->footer);
#endif
		foundat = bm_search(needle->footer,
							needle->footer_len,
							foundat,
							bytes_to_search,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);
	}

	if (foundat)
	{
#ifdef DEBUG
		printf("found %s!!!\n", needle->footer);
#endif
		if(needle->searchtype ==SEARCHTYPE_FORWARD_NEXT || needle->searchtype ==SEARCHTYPE_ASCII)
		{
				file_size = (foundat - buf);
		}
		else
		{
				file_size = (foundat - buf) + needle->footer_len;
		}	
	}
	else
	{
		file_size = needle->max_len;
	}

	if (file_size == 0)
	{
		file_size = needle->max_len;
	}

	if (file_size > (buflen-begin))
	{
		file_size = buflen;
	}
	
#ifdef DEBUG
	printf("The file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif

	extractbuf = buf;
	write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
	
	if(needle->searchtype !=SEARCHTYPE_ASCII)
	{
		foundat=buf;
		foundat += needle->header_len;
	}
	return foundat;		
	
	
	
}

/********************************************************************************
 *Function: extract_exe
 *Description:
 *Return: A pointer to where the EOF of the
 **********************************************************************************/
unsigned char *extract_exe(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	u_int64_t		file_size = 0;
	unsigned short	pe_offset = 0;
	unsigned int	SizeOfCode = 0;
	unsigned int	SizeOfInitializedData = 0;
	unsigned int	SizeOfUninitializedData = 0;
	unsigned int	rva = 0;
	unsigned int	offset = 0;
	unsigned short	sections = 0;
	unsigned int	sizeofimage = 0;
	unsigned int	raw_section_size = 0;
	unsigned int	size_of_headers = 0;
	unsigned short	dll = 0;
	unsigned int	sum = 0;
	unsigned short	exe_char = 0;
	unsigned int	align = 0;
	int				i = 0;
	time_t			compile_time = 0;
	struct tm		*ret_time;
	char			comment[32];
	char			ascii_time[32];

	if (buflen < 100)
		return foundat + 2;
	pe_offset = htos(&foundat[60], FOREMOST_LITTLE_ENDIAN);
	if (pe_offset < 1 || pe_offset > 1000 || pe_offset > buflen)
		{
		return foundat + 60;
		}

	foundat += pe_offset;
	if (foundat[0] != (unsigned char)'\x50' || foundat[1] != (unsigned char)'\x45')
		{
		return foundat;
		}

	sections = htos(&foundat[6], FOREMOST_LITTLE_ENDIAN);
	if (buflen < (40 * sections + 224))
		{
		return foundat;
		}

	compile_time = (time_t) htoi(&foundat[8], FOREMOST_LITTLE_ENDIAN);
	ret_time = gmtime(&compile_time);
	sprintf(ascii_time,
			"%02d/%02d/%04d %02d:%02d:%02d",
			ret_time->tm_mon + 1,
			ret_time->tm_mday,
			ret_time->tm_year + 1900,
			ret_time->tm_hour,
			ret_time->tm_min,
			ret_time->tm_sec);
	chop(ascii_time);

	sprintf(comment, ascii_time);
	strcat(needle->comment, comment);
	exe_char = htos(&foundat[22], FOREMOST_LITTLE_ENDIAN);
	if (exe_char & 0x2000)
		{
		dll = 1;
		}
	else if (exe_char & 0x1000)
		{

		//printf("System File!!!\n");
		}
	else if (exe_char & 0x0002)
		{

		//printf("EXE !!!\n");
		}
	else
		{
		return foundat;
		}

	foundat += 0x18;	/*Jump to opt header should be 0x0b 0x01*/

	SizeOfCode = htoi(&foundat[4], FOREMOST_LITTLE_ENDIAN);
	SizeOfInitializedData = htoi(&foundat[8], FOREMOST_LITTLE_ENDIAN);
	SizeOfUninitializedData = htoi(&foundat[12], FOREMOST_LITTLE_ENDIAN);
	rva = htoi(&foundat[16], FOREMOST_LITTLE_ENDIAN);
	align = htoi(&foundat[36], FOREMOST_LITTLE_ENDIAN);

	sizeofimage = htoi(&foundat[56], FOREMOST_LITTLE_ENDIAN);
	size_of_headers = htoi(&foundat[60], FOREMOST_LITTLE_ENDIAN);
	foundat += 224;

	/*Start of sections*/
	for (i = 0; i < sections; i++)
		{

		//strncpy(name,foundat,8);
		offset = htoi(&foundat[20], FOREMOST_LITTLE_ENDIAN);
		raw_section_size = htoi(&foundat[16], FOREMOST_LITTLE_ENDIAN);

		//printf("\t%s size=%d offset=%d\n",name,raw_section_size,offset);
		foundat += 40;

		//rem+=(raw_section_size%align);
		//sum+=raw_section_size;
		sum = offset + raw_section_size;
		}

	/*
    printf("rva is %d sum= %d\n",rva,sum);
    printf("soi is %d,soh is %d \n",sizeofimage,size_of_headers);
    printf("we are off by %d\n",sum-buflen);
    printf("soc=%d ,soidr=%d, souid=%d\n",SizeOfCode,SizeOfInitializedData,SizeOfUninitializedData);
    printf("fs=%d ,extr=%d\n",SizeOfCode+SizeOfInitializedData,SizeOfUninitializedData);
		*/
	file_size = sum;
	if (file_size < 512 || file_size > 4 * MEGABYTE)
		{
		return foundat + 60;
		}

	if (file_size > buflen)
		file_size = buflen;
	foundat = buf;
#ifdef DEBUG
	printf("The file size is  %llu  c_offset:=%llu\n", file_size, c_offset);
#endif

	extractbuf = buf;
	if (dll == 1)
		{
		strcpy(needle->suffix, "dll");
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		strcpy(needle->suffix, "exe");
		}
	else
		{
		write_to_disk(s, needle, file_size, extractbuf, c_offset + f_offset);
		}

	foundat += needle->header_len;
	return (buf + file_size);
}


/********************************************************************************
 *Function: extract_reg
 *Description:
 *Return: A pointer to where the EOF of the
 **********************************************************************************/
unsigned char *extract_reg(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	int sizeofreg = htoi(&foundat[0x28], FOREMOST_LITTLE_ENDIAN);
	int file_size=0;
	if(sizeofreg < 0 || sizeofreg > needle->max_len)	
	{
		return (foundat+4);
	}	
	foundat+=sizeofreg;
	file_size = (foundat - buf);

	extractbuf = buf;


	write_to_disk(s, needle, file_size , extractbuf, c_offset + f_offset);

			
	return NULL;
}
/********************************************************************************
 *Function: extract_rar
 *Description:
 *Return: A pointer to where the EOF of the
 **********************************************************************************/
unsigned char *extract_rar(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
						   s_spec *needle, u_int64_t f_offset)
{
	unsigned char	*buf = foundat;
	unsigned char	*extractbuf = NULL;
	u_int64_t		file_size = 0;
	unsigned short	headersize = 0;
	unsigned short	flags = 0;
	unsigned int	filesize = 0;
	unsigned int	tot_file_size = 0;
	unsigned int	ufilesize = 0;
	int				i = 0;
	int				scan = 0;
	int				flag = 0;
	int				passwd = 0;
	u_int64_t		bytes_to_search = 50 * KILOBYTE;
	char			comment[32];

	/*Marker Block*/
	headersize = htos(&foundat[5], FOREMOST_LITTLE_ENDIAN);
	foundat += headersize;

	/*Archive Block*/
	headersize = htos(&foundat[5], FOREMOST_LITTLE_ENDIAN);
	filesize = htoi(&foundat[7], FOREMOST_LITTLE_ENDIAN);

	if (foundat[2] != '\x73')
		{
		return foundat; /*Error*/
		}

	flags = htos(&foundat[3], FOREMOST_LITTLE_ENDIAN);
	if ((flags & 0x01) != 0)
		{
		sprintf(comment, " Multi-volume:");
		strcat(needle->comment, comment);
		}

	if (flags & 0x02)
		{
		sprintf(comment, " an archive comment is present:");
		strcat(needle->comment, comment);
		}

	foundat += headersize;

	if (foundat[2] != '\x74')
		{
		for (i = 0; i < 500; i++)
			{
			if (foundat[i] == '\x74')
				{
				foundat += i - 2;
				scan = 1;
				break;
				}
			}
		}

	if (headersize == 13 && foundat[2] != '\x74')
		{

		if (scan == 0)
			{
			sprintf(comment, "Encrypted Headers!");
			strcat(needle->comment, comment);
			}

		if (buflen - (foundat - buf) >= needle->max_len)
			bytes_to_search = needle->max_len;
		else
			bytes_to_search = buflen - (foundat - buf);

		//printf("bytes_to_search:=%d needle->footer_len:=%d needle->header_len:=%d\n",bytes_to_search,needle->footer_len,needle->header_len);
		foundat = bm_search(needle->footer,
							needle->footer_len,
							foundat,
							bytes_to_search,
							needle->footer_bm_table,
							needle->case_sen,
							SEARCHTYPE_FORWARD);
		if (foundat == NULL)
			{
			tot_file_size = bytes_to_search;
			foundat = buf + tot_file_size;
			}
		}
	else
		{

		/*Loop through files*/
		while (foundat[2] == '\x74')
			{

			headersize = htos(&foundat[5], FOREMOST_LITTLE_ENDIAN);
			filesize = htoi(&foundat[7], FOREMOST_LITTLE_ENDIAN);
			ufilesize = htoi(&foundat[11], FOREMOST_LITTLE_ENDIAN);

			if (headersize < 1 || headersize > buflen)
				flag = 1;
			if (filesize < 0 || filesize > buflen)
				flag = 1;
			if ((headersize + filesize) > buflen)
				flag = 1;
			if (ufilesize < 0)
				flag = 1;

			flags = htos(&foundat[3], FOREMOST_LITTLE_ENDIAN);
			if ((flags & 0x04) != 0)
				{
				passwd = 1;
				}

			tot_file_size = (foundat - buf);
			if ((tot_file_size + headersize + filesize) > buflen)
				{
				break;
				}

			foundat += headersize + filesize;
			}

		if (passwd == 1)
			{
			sprintf(comment, "Password Protected:");
			strcat(needle->comment, comment);
			}

		if (flag == 1)
			{
			sprintf(comment, "Encrypted Headers!");
			strcat(needle->comment, comment);
			foundat = bm_search(needle->footer,
								needle->footer_len,
								foundat,
								bytes_to_search,
								needle->footer_bm_table,
								needle->case_sen,
								SEARCHTYPE_FORWARD);
			if (foundat == NULL)
				{
				tot_file_size = bytes_to_search;
				foundat = buf + tot_file_size;
				}
			}

		if (foundat[2] != '\x7B' && tot_file_size == 0)
			{

			//printf("Error 7B!!!! %x\n",foundat[2]);
			return foundat;
			}

		foundat += 7;

		}

	if (foundat)
		{

		/*We found the EOF, write the file to disk and return*/
		tot_file_size = (foundat - buf);
		if (tot_file_size > buflen)
			file_size = buflen;

		extractbuf = buf;
		write_to_disk(s, needle, tot_file_size, extractbuf, c_offset + f_offset);
		return foundat;
		}
	else
		{
		return NULL;
		}

	return NULL;
}

unsigned char *extract_file(f_state *s, u_int64_t c_offset, unsigned char *foundat, u_int64_t buflen,
							s_spec *needle, u_int64_t f_offset)
{
	if (needle->type == JPEG)
		{
		return extract_jpeg(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == GIF)
		{
		return extract_gif(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == PNG)
		{
		return extract_png(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == BMP)
		{
		return extract_bmp(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == RIFF)
		{
		needle->suffix = "rif";
		return extract_riff(s, c_offset, foundat, buflen, needle, f_offset, "all");
		}
	else if (needle->type == AVI)
		{
		return extract_riff(s, c_offset, foundat, buflen, needle, f_offset, "avi");
		}
	else if (needle->type == WAV)
		{
		needle->suffix = "rif";
		return extract_riff(s, c_offset, foundat, buflen, needle, f_offset, "wav");
		}
	else if (needle->type == WMV)
		{
		return extract_wmv(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == OLE)
		{
		needle->suffix = "ole";
		return extract_ole(s, c_offset, foundat, buflen, needle, f_offset, "all");
		}
	else if (needle->type == DOC)
		{
		return extract_ole(s, c_offset, foundat, buflen, needle, f_offset, "doc");
		}
	else if (needle->type == PPT)
		{
		return extract_ole(s, c_offset, foundat, buflen, needle, f_offset, "ppt");
		}
	else if (needle->type == XLS)
		{
		needle->suffix = "ole";
		return extract_ole(s, c_offset, foundat, buflen, needle, f_offset, "xls");
		}
	else if (needle->type == PDF)
		{
		return extract_pdf(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == CPP)
		{
		return extract_cpp(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == HTM)
		{
		return extract_htm(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == MPG)
		{
		return extract_mpg(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == MP4)
		{
		return extract_mp4(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == ZIP)
		{
		return extract_zip(s, c_offset, foundat, buflen, needle, f_offset, "all");
		}
	else if (needle->type == RAR)
		{
		return extract_rar(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == SXW)
		{
		return extract_zip(s, c_offset, foundat, buflen, needle, f_offset, "sxw");
		}
	else if (needle->type == SXC)
		{
		return extract_zip(s, c_offset, foundat, buflen, needle, f_offset, "sxc");
		}
	else if (needle->type == SXI)
		{
		return extract_zip(s, c_offset, foundat, buflen, needle, f_offset, "sxi");
		}
	else if (needle->type == EXE)
		{
		return extract_exe(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == MOV || needle->type == VJPEG)
		{
		return extract_mov(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else if (needle->type == CONF)
		{
		return extract_generic(s, c_offset, foundat, buflen, needle, f_offset);
		}
	else
		{
		return NULL;
		}
	return NULL;	
}
