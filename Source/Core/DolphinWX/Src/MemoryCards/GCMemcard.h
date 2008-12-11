// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include "Common.h"
#include "StringUtil.h"

enum
{
	LENGTHFAIL = -4,
	GCSFAIL,
	SAVFAIL,
	OPENFAIL,
	GCI,
	HDR_READ_ERROR,
	DIR_READ_ERROR,
	DIR_BAK_READ_ERROR,
	BAT_READ_ERROR,
	BAT_BAK_READ_ERROR,
	HDR_CSUM_FAIL,
	DIR_CSUM_FAIL,
	BAT_CSUM_FAIL,
	DATA_READ_FAIL,
	HDR_SIZE_FFFF,
	NOTRAWORGCP,
	SAV = 0x80,
	GCS = 0x110,
	OUTOFBLOCKS,
	OUTOFDIRENTRIES,
	NOMEMCARD,
	NOFILE,
	INVALIDFILESIZE,
	TITLEPRESENT,
	SUCCESS = 0x2000,
	FAIL,
	MAXBLOCK = 0x2049
};

class GCMemcard 
{
private:
	void* mcdFile;

	u32 mc_data_size;
	u8* mc_data;

	void calc_checksumsBE(u16 *buf, u32 num, u16 *c1, u16 *c2);

#pragma pack(push,1)
	struct OSTime {
		u32 low;
		u32 high;
	};

	struct Header {			//Offset	Size	Description
		u8 Unk1[12];		//0x0000	12		?
		OSTime fmtTime;		//0x000c	8		time of format (OSTime value)
		u8 SramBias[4];		//0x0014	4		sram bias at time of format	
		u8 Unk2[8];			//0x0018	8	 	? almost always 0 or 1
		u8 Pad1[2];			//0x0020	2		padding zeroes
		u8 Size[2];			//0x0022	2		size of memcard in Mbits
		u8 Encoding[2];		//0x0024	2		encoding (ASCII or japanese)
		u8 Unused1[468];	//0x0026	468		unused (0xff)
		u8 UpdateCounter[2];//0x01fa	2		update Counter (?, probably unused)
		u8 CheckSum1[2];	//0x01fc	2		Checksum 1 (?)
		u8 CheckSum2[2];	//0x01fe	2		Checksum 2 (?)
		u8 Unused2[7680];	//0x0200	0x1e00	unused (0xff)
	} hdr;

	struct DEntry {
		u8 Gamecode[4];		//0x00		0x04	Gamecode
		u8 Markercode[2];	//0x04		0x02	Makercode
		u8 Unused1;			//0x06		0x01	reserved/unused (always 0xff, has no effect)
		u8 BIFlags;			//0x07		0x01	banner gfx format and icon animation (Image Key)
							//		bit(s)	description
							//		2		Icon Animation 0: forward 1: ping-pong
							//		1		[--0: No Banner 1: Banner present--] WRONG! YAGCD LIES!
							//		0		[--Banner Color 0: RGB5A3 1: CI8--]  WRONG! YAGCD LIES!
							//      bits 0 and 1: image format
							//		00 no banner
							//		01 CI8 banner
							//		01 RGB5A3 banner
							//		11 ? maybe ==01? haven't seen it
							// 	 	
		u8 Filename[32];	//0x08		0x20	filename
		u8 ModTime[4];		//0x28		0x04	Time of file's last modification in seconds since 12am, January 1st, 2000
		u8 ImageOffset[4];	//0x2c		0x04	image data offset
		u8 IconFmt[2];		//0x30		0x02	icon gfx format (2bits per icon)
							//		bits	Description
							//		00	no icon
							//		01	CI8 with a shared color palette after the last frame
							//		10	RGB5A3
							//		11	CI8 with a unique color palette after itself
							// 	 	
		u8 AnimSpeed[2];	//0x32		0x02	animation speed (2bits per icon) (*1)
							//		bits	Description
							//		00	no icon
							//		01	Icon lasts for 4 frames
							//		10	Icon lasts for 8 frames
							//		11	Icon lasts for 12 frames
							// 	 	
		u8 Permissions;		//0x34		0x01	file-permissions
							//		bit	permission	Description
							//		4	no move		File cannot be moved by the IPL
							//		3	no copy		File cannot be copied by the IPL
							//		2	public		Can be read by any game
							//	 	
		u8 CopyCounter;		//0x35		0x01	copy counter (*2)
		u8 FirstBlock[2];	//0x36		0x02	block no of first block of file (0 == offset 0)
		u8 BlockCount[2];	//0x38		0x02	file-length (number of blocks in file)
		u8 Unused2[2];		//0x3a		0x02	reserved/unused (always 0xffff, has no effect)
		u8 CommentsAddr[4];	//0x3c		0x04	Address of the two comments within the file data (*3)
	};

	struct Directory {
		DEntry Dir[127];	//0x0000	 	Directory Entries (max 127)
		u8 Padding[0x3a];
		u8 UpdateCounter[2];//0x1ffa	2	update Counter
		u8 CheckSum1[2];	//0x1ffc	2	Checksum 1
		u8 CheckSum2[2];	//0x1ffe	2	Checksum 2
	} dir, dir_backup;

	struct BlockAlloc {
		u8 CheckSum1[2];	//0x0000	2	Checksum 1
		u8 CheckSum2[2];	//0x0002	2	Checksum 2
		u8 UpdateCounter[2];//0x0004	2	update Counter
		u8 FreeBlocks[2];	//0x0006	2	free Blocks
		u8 LastAllocated[2];//0x0008	2	last allocated Block
		u16 Map[0xFFB];		//0x000a	0x1ff8	Map of allocated Blocks
	} bat,bat_backup;
#pragma pack(pop)

public:
	bool fail[12];

	// constructor
	GCMemcard(const char* fileName);

	// destructor
	~GCMemcard();

	bool IsOpen();

	u32  TestChecksums();
	bool FixChecksums();
	
	// get number of file entries in the directory
	u32  GetNumFiles();

	// Returns true if title already on memcard
	bool TitlePresent(DEntry d);
	
	// read directory entry
	bool GetFileInfo(u32 index, DEntry& data);

	// buffer needs to be a char[32] or bigger
	bool GetFileName(u32 index, char* buffer);

	// get file length in blocks
	u16  GetFileSize(u32 index);

	// get first block for file
	u16 GetFirstBlock(u32 index);

	// get the free blocks from bat
	u16 GetFreeBlocks(void);

	// buffer needs to be a char[32] or bigger
	bool GetComment1(u32 index, char* buffer);

	// buffer needs to be a char[32] or bigger
	bool GetComment2(u32 index, char* buffer);

	// assumes there's enough space in buffer
	// old determines if function uses old or new method of copying data
	// some functions only work with old way, some only work with new way
	// TODO: find a function that works for all calls or split into 2 functions
	u32 GetFileData(u32 index, u8* buffer, bool old);

	// delete a file from the directory
	u32 RemoveFile(u32 index);
	
	// adds the file to the directory and copies its contents
	// if remove > 0 it will pad bat.map with 0's sifeof remove
	u32  ImportFile(DEntry& direntry, u8* contents, int remove);

	// reads a save from another memcard, and imports the data into this memcard
	u32  CopyFrom(GCMemcard& source, u32 index);

	// writes a .gci file to disk containing index
	u32 ExportGci(u32 index, const char* fileName);

	// reads a .gci/.gcs/.sav file and calls ImportFile or saves out a gci file
	s32  ImportGci(const char* fileName, std::string fileName2);

	// reads the banner image
	bool ReadBannerRGBA8(u32 index, u32* buffer);

	// reads the animation frames
	u32  ReadAnimRGBA8(u32 index, u32* buffer, u8 *delays);

	bool Save();

};
