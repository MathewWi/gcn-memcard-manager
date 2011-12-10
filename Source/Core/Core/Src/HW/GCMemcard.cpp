// Copyright (C) 2003 Dolphin Project.

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
#include "GCMemcard.h"
#include "ColorUtil.h"
static void ByteSwap(u8 *valueA, u8 *valueB)
{
	u8 tmp = *valueA;
	*valueA = *valueB;
	*valueB = tmp;
}

void decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = ColorUtil::Decode5A3(Common::swap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
}

void decodeCI8image(u32* dst, u8* src, u16* pal, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 8)
		{
			for (int iy = 0; iy < 4; iy++, src += 8)
			{
				u32 *tdst = dst+(y+iy)*width+x;
				for (int ix = 0; ix < 8; ix++)
				{
					// huh, this seems wrong. CI8, not 5A3, no?
					tdst[ix] = ColorUtil::Decode5A3(Common::swap16(pal[src[ix]]));
				}
			}
		}
	}
}

GCMemcard::GCMemcard(const char *filename, bool forceCreation, bool sjis)
	: m_valid(false)
	, m_fileName(filename)
{ 
	File::IOFile mcdFile(m_fileName, "r+b");
	if (!mcdFile.IsOpen())
	{
		if (!forceCreation && !AskYesNoT("\"%s\" does not exist.\n Create a new 16MB Memcard?", filename))
		{
			return;
		}
		Format(forceCreation ? sjis : !AskYesNoT("Format as ascii (NTSC\\PAL)?\nChoose no for sjis (NTSC-J)"));
		return;
	}
	else
	{
		//This function can be removed once more about hdr is known and we can check for a valid header
		std::string fileType;
		SplitPath(filename, NULL, NULL, &fileType);
		if (strcasecmp(fileType.c_str(), ".raw") && strcasecmp(fileType.c_str(), ".gcp"))
		{
			PanicAlertT("File has the extension \"%s\"\nvalid extensions are (.raw/.gcp)", fileType.c_str());
			return;
		}
		u32 size = mcdFile.GetSize();
		if (size < MC_FST_BLOCKS*BLOCK_SIZE)
		{
			PanicAlertT("%s failed to load as a memorycard \nfile is not large enough to be a valid memory card file (0x%x bytes)", filename, size);
			return;
		}
		if (size % BLOCK_SIZE)
		{
			PanicAlertT("%s failed to load as a memorycard \n Card file size is invalid (0x%x bytes)", filename, size);
				return;
		}

		m_sizeMb = (size/BLOCK_SIZE) / MBIT_TO_BLOCKS;
		switch (m_sizeMb)
		{
			case MemCard59Mb:
			case MemCard123Mb:
			case MemCard251Mb:
			case Memcard507Mb:
			case MemCard1019Mb:
			case MemCard2043Mb:
				break;
			default:
				PanicAlertT("%s failed to load as a memorycard \n Card size is invalid (0x%x bytes)", filename, size);
				return;
		}
	}
	
	
	mcdFile.Seek(0, SEEK_SET);
	if (!mcdFile.ReadBytes(&hdr, BLOCK_SIZE))
	{
		PanicAlertT("Failed to read header correctly\n(0x0000-0x1FFF)");
		return;
	}
	if (m_sizeMb != BE16(hdr.SizeMb))
	{
		PanicAlertT("Memorycard filesize does not match the header size");
		return;
	}

	if (!mcdFile.ReadBytes(&dir, BLOCK_SIZE))
	{
		PanicAlertT("Failed to read directory correctly\n(0x2000-0x3FFF)");
		return;
	}

	if (!mcdFile.ReadBytes(&dir_backup, BLOCK_SIZE))
	{
		PanicAlertT("Failed to read directory backup correctly\n(0x4000-0x5FFF)");
		return;
	}

	if (!mcdFile.ReadBytes(&bat, BLOCK_SIZE))
	{
		PanicAlertT("Failed to read block allocation table correctly\n(0x6000-0x7FFF)");
		return;
	}

	if (!mcdFile.ReadBytes(&bat_backup, BLOCK_SIZE))
	{
		PanicAlertT("Failed to read block allocation table backup correctly\n(0x8000-0x9FFF)");
		return;
	}

	u32 csums = TestChecksums();
	
	if (csums & 0x1)
	{
		// header checksum error!
		// invalid files do not always get here
		PanicAlertT("Header checksum failed");
		return;
	}

	if (csums & 0x2) // directory checksum error!
	{
		if (csums & 0x4)
		{
			// backup is also wrong!
			PanicAlertT("Directory checksum failed\n and Directory backup checksum failed");
			return;
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
	}

	if (csums & 0x8) // BAT checksum error!
	{
		if (csums & 0x10)
		{
			// backup is also wrong!
			PanicAlertT("Block Allocation Table checksum failed");
			return;
		}
		else
		{
			// backup is correct, restore
			dir = dir_backup;
			bat = bat_backup;

			// update checksums
			csums = TestChecksums();
		}
// It seems that the backup having a larger counter doesn't necessarily mean
// the backup should be copied?
//	}
//
//	if(BE16(dir_backup.UpdateCounter) > BE16(dir.UpdateCounter)) //check if the backup is newer
//	{
//		dir = dir_backup;
//		bat = bat_backup; // needed?
	}

	mcdFile.Seek(0xa000, SEEK_SET);
	
	maxBlock = (u32)m_sizeMb * MBIT_TO_BLOCKS;
	mc_data_blocks.reserve(maxBlock - MC_FST_BLOCKS);

	m_valid = true;
	for (u32 i = MC_FST_BLOCKS; i < maxBlock; ++i)
	{
		GCMBlock b;
		if (mcdFile.ReadBytes(b.block, BLOCK_SIZE))
		{
			mc_data_blocks.push_back(b);
		}
		else
		{
			PanicAlertT("Failed to read block %d of the save data\nMemcard may be truncated\nFilePosition:%llx", i, mcdFile.Tell());
			m_valid = false;
			break;
		}
	}

	mcdFile.Close();
}

bool GCMemcard::IsAsciiEncoding() const
{
	return hdr.Encoding == 0;
}

bool GCMemcard::Save()
{
	File::IOFile mcdFile(m_fileName, "wb");
	mcdFile.Seek(0, SEEK_SET);

	mcdFile.WriteBytes(&hdr, BLOCK_SIZE);
	mcdFile.WriteBytes(&dir, BLOCK_SIZE);
	mcdFile.WriteBytes(&dir_backup, BLOCK_SIZE);
	mcdFile.WriteBytes(&bat, BLOCK_SIZE);
	mcdFile.WriteBytes(&bat_backup, BLOCK_SIZE);
	for (int i = 0; i < maxBlock - MC_FST_BLOCKS; ++i)
	{
		mcdFile.WriteBytes(mc_data_blocks[i].block, BLOCK_SIZE);
	}

	return mcdFile.Close();
}

void GCMemcard::calc_checksumsBE(u16 *buf, u32 length, u16 *csum, u16 *inv_csum)
{
	*csum = *inv_csum = 0;

	for (u32 i = 0; i < length; ++i)
	{
		//weird warnings here
		*csum += BE16(buf[i]);
		*inv_csum += BE16((u16)(buf[i] ^ 0xffff));
	}
	*csum = BE16(*csum);
	*inv_csum = BE16(*inv_csum);
	if (*csum == 0xffff)
	{
		*csum = 0;
	}
	if (*inv_csum == 0xffff)
	{
		*inv_csum = 0;
	}
}

u32  GCMemcard::TestChecksums() const
{
	u16 csum=0,
		csum_inv=0;

	u32 results = 0;

	calc_checksumsBE((u16*)&hdr, 0xFE , &csum, &csum_inv);
	if ((hdr.Checksum != csum) || (hdr.Checksum_Inv != csum_inv)) results |= 1;

	calc_checksumsBE((u16*)&dir, 0xFFE, &csum, &csum_inv);
	if ((dir.Checksum != csum) || (dir.Checksum_Inv != csum_inv)) results |= 2;

	calc_checksumsBE((u16*)&dir_backup, 0xFFE, &csum, &csum_inv);
	if ((dir_backup.Checksum != csum) || (dir_backup.Checksum_Inv != csum_inv)) results |= 4;

	calc_checksumsBE((u16*)(((u8*)&bat)+4), 0xFFE, &csum, &csum_inv);
	if ((bat.Checksum != csum) || (bat.Checksum_Inv != csum_inv)) results |= 8;

	calc_checksumsBE((u16*)(((u8*)&bat_backup)+4), 0xFFE, &csum, &csum_inv);
	if ((bat_backup.Checksum != csum) || (bat_backup.Checksum_Inv != csum_inv)) results |= 16;

	return results;
}

bool GCMemcard::FixChecksums()
{
	if (!m_valid)
		return false;
	
	calc_checksumsBE((u16*)&hdr, 0xFE, &hdr.Checksum, &hdr.Checksum_Inv);
	calc_checksumsBE((u16*)&dir, 0xFFE, &dir.Checksum, &dir.Checksum_Inv);
	calc_checksumsBE((u16*)&dir_backup, 0xFFE, &dir_backup.Checksum, &dir_backup.Checksum_Inv);
	calc_checksumsBE((u16*)&bat+2, 0xFFE, &bat.Checksum, &bat.Checksum_Inv);
	calc_checksumsBE((u16*)&bat_backup+2, 0xFFE, &bat_backup.Checksum, &bat_backup.Checksum_Inv);

	return true;
}

u8 GCMemcard::GetNumFiles() const
{
	if (!m_valid)
		return 0;

	u8 j = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode)!= 0xFFFFFFFF)
			 j++;
	}
	return j;
}

u8 GCMemcard::GetFileIndex(u8 fileNumber) const
{
	if (m_valid)
	{

		u8 j = 0;
		for (u8 i = 0; i < DIRLEN; i++)
		{
			if (BE32(dir.Dir[i].Gamecode)!= 0xFFFFFFFF)
			{
				if (j == fileNumber)
				{
					return i;
				}
				j++;
			}
		}
	}
	return 0xFF;
}

u16 GCMemcard::GetFreeBlocks() const
{
	if (!m_valid)
		return 0;

	return BE16(bat.FreeBlocks);
}

u8 GCMemcard::TitlePresent(DEntry d) const
{
	if (!m_valid)
		return DIRLEN;

	u8 i = 0;
	while(i < DIRLEN)
	{
		if ((BE32(dir.Dir[i].Gamecode) == BE32(d.Gamecode)) &&
			(!memcmp(dir.Dir[i].Filename, d.Filename, 32)))
		{
			break;
		}
		i++;
	}
	return i;
}

bool GCMemcard::GCI_FileName(u8 index, std::string &filename) const
{
	if (!m_valid || index > DIRLEN || (BE32(dir.Dir[index].Gamecode) == 0xFFFFFFFF))
		return false;
	filename = std::string((char*)dir.Dir[index].Gamecode, 4) + '_' + (char*)dir.Dir[index].Filename + ".gci";
	return true;
}

// DEntry functions, all take u8 index < DIRLEN (127)
// Functions that have ascii output take a char *buffer

std::string GCMemcard::DEntry_GameCode(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";
	return std::string((const char*)dir.Dir[index].Gamecode, 4);
}

std::string GCMemcard::DEntry_Makercode(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";
	return std::string((const char*)dir.Dir[index].Makercode, 2);
}

std::string GCMemcard::DEntry_BIFlags(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";

	std::string flags;
	int x = dir.Dir[index].BIFlags;
	for (int i = 0; i < 8; i++)
	{
		flags.push_back((x & 0x80) ? '1' : '0');
		x = x << 1;
	}
	flags.push_back(0);
	return flags;
}

std::string GCMemcard::DEntry_FileName(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";
	return std::string((const char*)dir.Dir[index].Filename, DENTRY_STRLEN);
}

u32 GCMemcard::DEntry_ModTime(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFFFFFFFF;
	return BE32(dir.Dir[index].ModTime);
}

u32 GCMemcard::DEntry_ImageOffset(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFFFFFFFF;
	return BE32(dir.Dir[index].ImageOffset);
}

std::string GCMemcard::DEntry_IconFmt(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";
	int x = dir.Dir[index].IconFmt[0];
	std::string format;
	for(int i = 0; i < 16; i++)
	{
		if (i == 8) x = dir.Dir[index].IconFmt[1];
		format.push_back((x & 0x80) ? '1' : '0');
		x = x << 1;
	}
	format.push_back(0);
	return format;
}

u16 GCMemcard::DEntry_AnimSpeed(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFF;
	return BE16(dir.Dir[index].AnimSpeed);
}

std::string GCMemcard::DEntry_Permissions(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";
	u8 Permissions = dir.Dir[index].Permissions;
	std::string permissionsString;
	permissionsString.push_back((Permissions & 16) ? 'x' : 'M');
	permissionsString.push_back((Permissions &  8) ? 'x' : 'C');
	permissionsString.push_back((Permissions &  4) ? 'P' : 'x');
	permissionsString.push_back(0);
	return permissionsString;
}

u8 GCMemcard::DEntry_CopyCounter(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFF;
	return dir.Dir[index].CopyCounter;
}

u16 GCMemcard::DEntry_FirstBlock(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFFFF;

	u16 block = BE16(dir.Dir[index].FirstBlock);
	if (block > (u16) maxBlock) return 0xFFFF;
	return block;
}

u16 GCMemcard::DEntry_BlockCount(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFFFF;

	u16 blocks = BE16(dir.Dir[index].BlockCount);
	if (blocks > (u16) maxBlock) return 0xFFFF;
	return blocks;
}

u32 GCMemcard::DEntry_CommentsAddress(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return 0xFFFF;
	return BE32(dir.Dir[index].CommentsAddr);
}

std::string GCMemcard::GetSaveComment1(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";

	u32 Comment1 = BE32(dir.Dir[index].CommentsAddr);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;
	if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
	{
		return "";
	}
	return std::string((const char *)mc_data_blocks[DataBlock].block + Comment1, DENTRY_STRLEN);
}

std::string GCMemcard::GetSaveComment2(u8 index) const
{
	if (!m_valid || index > DIRLEN)
		return "";

	u32 Comment1 = BE32(dir.Dir[index].CommentsAddr);
	u32 Comment2 = Comment1 + DENTRY_STRLEN;
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;
	if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
	{
		return "";
	}
	return std::string((const char *)mc_data_blocks[DataBlock].block + Comment2, DENTRY_STRLEN);
}

bool GCMemcard::GetDEntry(u8 index, DEntry &dest) const
{
	if (!m_valid || index > DIRLEN)
		return false;
	dest = dir.Dir[index];
	return true;
}

u32 GCMemcard::GetSaveData(u8 index,  std::vector<GCMBlock> & Blocks) const
{
	if (!m_valid)
		return NOMEMCARD;

	u16 block = DEntry_FirstBlock(index);
	u16 saveLength = DEntry_BlockCount(index);
	u16 memcardSize = BE16(hdr.SizeMb) * MBIT_TO_BLOCKS;

	if ((block == 0xFFFF) || (saveLength == 0xFFFF)
		|| (block + saveLength > memcardSize))
	{
		return FAIL;
	}

	for (int i = 0; i < saveLength; ++i)
	{
		Blocks.push_back(mc_data_blocks[i + block-MC_FST_BLOCKS]);
	}
	return SUCCESS;
}
// End DEntry functions

u32 GCMemcard::ImportFile(DEntry& direntry, std::vector<GCMBlock> &saveBlocks, int remove)
{
	if (!m_valid)
		return NOMEMCARD;

	if (GetNumFiles() >= DIRLEN)
	{
		return OUTOFDIRENTRIES;
	}
	if (BE16(bat.FreeBlocks) < BE16(direntry.BlockCount))
	{
		return OUTOFBLOCKS;
	}
	if (!remove && (TitlePresent(direntry) != DIRLEN))
	{
		return TITLEPRESENT;
	}

	// find first free data block -- assume no freespace fragmentation
	int totalspace = (((u32)BE16(hdr.SizeMb) * MBIT_TO_BLOCKS) - MC_FST_BLOCKS);
	int firstFree1 = BE16(bat.LastAllocated) + 1;
	int firstFree2 = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode) == 0xFFFFFFFF)
		{
			break;
		}
		else
		{
			firstFree2 = max<int>(firstFree2,
				(int)(BE16(dir.Dir[i].FirstBlock) + BE16(dir.Dir[i].BlockCount)));
		}
	}
	firstFree1 = max<int>(firstFree1, firstFree2);

	// find first free dir entry
	int index = -1;
	for (int i=0; i < DIRLEN; i++)
	{
		if (BE32(dir.Dir[i].Gamecode) == 0xFFFFFFFF)
		{
			index = i;
			dir.Dir[i] = direntry;
			*(u16*)&dir.Dir[i].FirstBlock = BE16(firstFree1);
			if (!remove)
			{
				dir.Dir[i].CopyCounter = dir.Dir[i].CopyCounter+1;
			}
			dir_backup = dir;
			break;
		}
	}


	int fileBlocks = BE16(direntry.BlockCount);
	firstFree1 -= MC_FST_BLOCKS;
	// keep assuming no freespace fragmentation, and copy over all the data
	for (int i = 0; i < fileBlocks; ++i)
	{
		mc_data_blocks[i + firstFree1] = saveBlocks[i];
	}

	bat_backup = bat;
	u16 last = BE16(bat_backup.LastAllocated);
	u16 i = (last - 4);
	int j = 2;
	while(j < BE16(direntry.BlockCount) + 1)
	{
		bat_backup.Map[i] = BE16(last + (u16)j);
		i++;
		j++;
	}
	bat_backup.Map[i++] = 0xFFFF;
	//Set bat.map to 0 for each block that was removed
	for (int k = 0; k < remove; k++)
	{
		bat_backup.Map[i++] = 0x0000;
	}

	//update last allocated block
	*(u16*)&bat_backup.LastAllocated = BE16(BE16(bat_backup.LastAllocated) + j - 1);
	//update freespace counter
	*(u16*)&bat_backup.FreeBlocks = BE16(totalspace - firstFree1 - fileBlocks + MC_FST_BLOCKS);

	
	if (!remove)
	{	// ... and dir update counter
		*(u16*)&dir_backup.UpdateCounter = BE16(BE16(dir_backup.UpdateCounter) + 1);
		// ... and bat update counter
		*(u16*)&bat_backup.UpdateCounter = BE16(BE16(bat_backup.UpdateCounter) + 1);
	}
	bat = bat_backup;
	return SUCCESS;
}

u32 GCMemcard::RemoveFile(u8 index) //index in the directory array
{
	if (!m_valid)
		return NOMEMCARD;

	//error checking
	u16 startingblock = 0;
	for (int i = 0; i < DIRLEN; i++)
	{
		if (startingblock > BE16(dir.Dir[i].FirstBlock))
			return DELETE_FAIL;
		startingblock = BE16(dir.Dir[i].FirstBlock);
	}

	//backup the directory and bat (not really needed here but meh :P
	dir_backup = dir;
	bat_backup = bat;

	//free the blocks
	int blocks_left = BE16(dir.Dir[index].BlockCount);
	*(u16*)&bat.LastAllocated = BE16(BE16(dir.Dir[index].FirstBlock) - 1);

	u8 nextIndex = index + 1;
	memset(&(dir.Dir[index]), 0xFF, DENTRY_SIZE);

	while (nextIndex < DIRLEN)
	{
		DEntry tempDEntry;
		GetDEntry(nextIndex, tempDEntry);
		std::vector<GCMBlock> saveData;
		u16 size = 0;
		//Only get file data if it is a valid dir entry
		if (BE16(tempDEntry.FirstBlock) != 0xFFFF)
		{
			*(u16*)&bat.FreeBlocks = BE16(BE16(bat.FreeBlocks) - BE16(tempDEntry.BlockCount));

			size = DEntry_BlockCount(nextIndex);
			if (size != 0xFFFF)
			{
				saveData.reserve(size);
				switch (GetSaveData(nextIndex, saveData))
				{
				case NOMEMCARD:
					break;
				case FAIL:
					return FAIL;
				}
			}
		}
		memset(&(dir.Dir[nextIndex]), 0xFF, DENTRY_SIZE);
		//Only call import file if GetSaveData returns SUCCESS
		if (saveData.size() == size)
		{
			ImportFile(tempDEntry, saveData, blocks_left);
		}
		nextIndex++;

	}
	//Added to clean up if removing last file
	if (BE16(bat.LastAllocated) == (u16)4)
	{
		for (int j = 0; j < blocks_left; j++)
		{
			bat.Map[j] = 0x0000;
		}
	}
	// increment update counter
	*(u16*)&dir.UpdateCounter = BE16(BE16(dir.UpdateCounter) + 1);

	return SUCCESS;
}

u32 GCMemcard::CopyFrom(const GCMemcard& source, u8 index)
{
	if (!m_valid || !source.m_valid)
		return NOMEMCARD;

	DEntry tempDEntry;
	if (!source.GetDEntry(index, tempDEntry))
		return NOMEMCARD;
	
	u32 size = source.DEntry_BlockCount(index);
	if (size == 0xFFFF) return INVALIDFILESIZE;

	std::vector<GCMBlock> saveData;
	saveData.reserve(size);
	switch (source.GetSaveData(index, saveData))
	{
	case FAIL:
		return FAIL;
	case NOMEMCARD:
		return NOMEMCARD;
	default:
		return ImportFile(tempDEntry, saveData, 0);
	}
}

u32 GCMemcard::ImportGci(const char *inputFile, const std::string &outputFile)
{
	if (outputFile.empty() && !m_valid)
		return OPENFAIL;

	File::IOFile gci(inputFile, "rb");
	if (!gci)
		return OPENFAIL;

	u32 result = ImportGciInternal(gci.ReleaseHandle(), inputFile, outputFile);

	return result;
}

u32 GCMemcard::ImportGciInternal(FILE* gcih, const char *inputFile, const std::string &outputFile)
{
	File::IOFile gci(gcih);
	unsigned int offset;
	char tmp[0xD];
	std::string fileType;
	SplitPath(inputFile, NULL, NULL, &fileType);

	if (!strcasecmp(fileType.c_str(), ".gci"))
		offset = GCI;
	else
	{
		gci.ReadBytes(tmp, 0xD);
		if (!strcasecmp(fileType.c_str(), ".gcs"))
		{
			if (!memcmp(tmp, "GCSAVE", 6))	// Header must be uppercase
				offset = GCS;
			else
				return GCSFAIL;
		}
		else if (!strcasecmp(fileType.c_str(), ".sav"))
		{
			if (!memcmp(tmp, "DATELGC_SAVE", 0xC)) // Header must be uppercase
				offset = SAV;
			else
				return SAVFAIL;
		}
		else
			return OPENFAIL;
	}
	gci.Seek(offset, SEEK_SET);

	DEntry tempDEntry;
	gci.ReadBytes(&tempDEntry, DENTRY_SIZE);
	const int fStart = (int)gci.Tell();
	gci.Seek(0, SEEK_END);
	const int length = (int)gci.Tell() - fStart;
	gci.Seek(offset + DENTRY_SIZE, SEEK_SET);

	Gcs_SavConvert(tempDEntry, offset, length);

	if (length != BE16(tempDEntry.BlockCount) * BLOCK_SIZE)
		return LENGTHFAIL;
	if (gci.Tell() != offset + DENTRY_SIZE) // Verify correct file position
		return OPENFAIL;
	
	u32 size = BE16((tempDEntry.BlockCount));
	std::vector<GCMBlock> saveData;
	saveData.reserve(size);

	for (int i = 0; i < size; ++i)
	{
		GCMBlock b;
		gci.ReadBytes(b.block, BLOCK_SIZE);
		saveData.push_back(b);
	}
	u32 ret;
	if (!outputFile.empty())
	{
		File::IOFile gci2(outputFile, "wb");
		bool completeWrite = true;
		if (!gci2)
		{
			return OPENFAIL;
		}
		gci2.Seek(0, SEEK_SET);

		if (!gci2.WriteBytes(&tempDEntry, DENTRY_SIZE)) 
			completeWrite = false;
		int fileBlocks = BE16(tempDEntry.BlockCount);
		gci2.Seek(DENTRY_SIZE, SEEK_SET);

		for (int i = 0; i < fileBlocks; ++i)
		{
			if (!gci2.WriteBytes(saveData[i].block, BLOCK_SIZE))
			completeWrite = false;
		}

		if (completeWrite)
			ret = GCS;
		else
			ret = WRITEFAIL;
	}
	else 
		ret = ImportFile(tempDEntry, saveData, 0);

	return ret;
}

u32 GCMemcard::ExportGci(u8 index, const char *fileName, const std::string &directory) const
{
	File::IOFile gci;
	int offset = GCI;
	if (!fileName)
	{
		std::string gciFilename;
		if (!GCI_FileName(index, gciFilename)) return SUCCESS;
		gci.Open(directory + DIR_SEP + gciFilename, "wb");
	}
	else
	{
		gci.Open(fileName, "wb");

		std::string fileType;
		SplitPath(fileName, NULL, NULL, &fileType);
		if (!strcasecmp(fileType.c_str(), ".gcs"))
		{
			offset = GCS;
		}
		else if (!strcasecmp(fileType.c_str(), ".sav"))
		{
			offset = SAV;
		}
	}

	if (!gci)
		return OPENFAIL;

	gci.Seek(0, SEEK_SET);
	
	switch(offset)
	{
	case GCS:
		u8 gcsHDR[GCS];
		memset(gcsHDR, 0, GCS);
		memcpy(gcsHDR, "GCSAVE", 6);
		gci.WriteArray(gcsHDR, GCS);
		break;

	case SAV:
		u8 savHDR[SAV];
		memset(savHDR, 0, SAV);
		memcpy(savHDR, "DATELGC_SAVE", 0xC);
		gci.WriteArray(savHDR, SAV);
		break;
	}

	DEntry tempDEntry;
	if (!GetDEntry(index, tempDEntry))
	{
		return NOMEMCARD;
	}

	Gcs_SavConvert(tempDEntry, offset);
	gci.WriteBytes(&tempDEntry, DENTRY_SIZE);

	u32 size = DEntry_BlockCount(index);
	if (size == 0xFFFF)
	{
		return FAIL;
	}

	std::vector<GCMBlock> saveData;
	saveData.reserve(size);

	switch(GetSaveData(index, saveData))
	{
	case FAIL:
		return FAIL;
	case NOMEMCARD:
		return NOMEMCARD;
	}
	gci.Seek(DENTRY_SIZE + offset, SEEK_SET);
	for (int i = 0; i < size; ++i)
	{
		gci.WriteBytes(saveData[i].block, BLOCK_SIZE);
	}

	if (gci.IsGood())
		return SUCCESS;
	else
		return WRITEFAIL;
}

void GCMemcard::Gcs_SavConvert(DEntry &tempDEntry, int saveType, int length)
{
	switch(saveType)
	{
	case GCS:
	{	// field containing the Block count as displayed within
		// the GameSaves software is not stored in the GCS file.
		// It is stored only within the corresponding GSV file.
		// If the GCS file is added without using the GameSaves software,
		// the value stored is always "1"
		*(u16*)&tempDEntry.BlockCount = BE16(length / BLOCK_SIZE);
	}
		break;
	case SAV:
		// swap byte pairs
		// 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
		// 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
		// 0x3C and 0x3D,0x3E and 0x3F.
		// It seems that sav files also swap the BIFlags...
		ByteSwap(&tempDEntry.Unused1, &tempDEntry.BIFlags);
		ArrayByteSwap((tempDEntry.ImageOffset));
		ArrayByteSwap(&(tempDEntry.ImageOffset[2]));
		ArrayByteSwap((tempDEntry.IconFmt));
		ArrayByteSwap((tempDEntry.AnimSpeed));
		ByteSwap(&tempDEntry.Permissions, &tempDEntry.CopyCounter);
		ArrayByteSwap((tempDEntry.FirstBlock));
		ArrayByteSwap((tempDEntry.BlockCount));
		ArrayByteSwap((tempDEntry.Unused2));
		ArrayByteSwap((tempDEntry.CommentsAddr));
		ArrayByteSwap(&(tempDEntry.CommentsAddr[2]));
		break;
	default:
		break;
	}
}

bool GCMemcard::ReadBannerRGBA8(u8 index, u32* buffer) const
{
	if (!m_valid)
		return false;

	int flags = dir.Dir[index].BIFlags;
	// Timesplitters 2 is the only game that I see this in
	// May be a hack
	if (flags == 0xFB) flags = ~flags;

	int  bnrFormat = (flags&3);

	if (bnrFormat == 0)
		return false;

	u32 DataOffset = BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;

	if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
	{
		return false;
	}

	const int pixels = 96*32;

	if (bnrFormat&1)
	{
		u8  *pxdata  = (u8* )(mc_data_blocks[DataBlock].block + DataOffset);
		u16 *paldata = (u16*)(mc_data_blocks[DataBlock].block + DataOffset + pixels);

		decodeCI8image(buffer, pxdata, paldata, 96, 32);
	}
	else
	{
		u16 *pxdata = (u16*)(mc_data_blocks[DataBlock].block + DataOffset);

		decode5A3image(buffer, pxdata, 96, 32);
	}
	return true;
}

u32 GCMemcard::ReadAnimRGBA8(u8 index, u32* buffer, u8 *delays) const
{
	if (!m_valid)
		return 0;

	// To ensure only one type of icon is used
	// Sonic Heroes it the only game I have seen that tries to use a CI8 and RGB5A3 icon
	int fmtCheck = 0; 

	int formats = BE16(dir.Dir[index].IconFmt);
	int fdelays  = BE16(dir.Dir[index].AnimSpeed);

	int flags = dir.Dir[index].BIFlags;
	// Timesplitters 2 is the only game that I see this in
	// May be a hack
	if (flags == 0xFB) flags = ~flags;
	int bnrFormat = (flags&3);

	u32 DataOffset = BE32(dir.Dir[index].ImageOffset);
	u32 DataBlock = BE16(dir.Dir[index].FirstBlock) - MC_FST_BLOCKS;

	if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
	{
		return 0;
	}

	u8* animData = (u8*)(mc_data_blocks[DataBlock].block + DataOffset);

	switch (bnrFormat)
	{
	case 1:
	case 3:
		animData += 96*32 + 2*256; // image+palette
		break;
	case 2:
		animData += 96*32*2;
		break;
	}

	int fmts[8];
	u8* data[8];
	int frames = 0;


	for (int i = 0; i < 8; i++)
	{
		fmts[i] = (formats >> (2*i))&3;
		delays[i] = ((fdelays >> (2*i))&3) << 2;
		data[i] = animData;

		if (!fmtCheck) fmtCheck = fmts[i];
		if (fmtCheck == fmts[i])
		{
			switch (fmts[i])
			{
			case CI8SHARED: // CI8 with shared palette
				animData += 32*32;
				frames++;
				break;
			case RGB5A3: // RGB5A3
				animData += 32*32*2;
				frames++;
				break;
			case CI8: // CI8 with own palette
				animData += 32*32 + 2*256;
				frames++;
				break;
			}
		}
	}

	u16* sharedPal = (u16*)(animData);

	for (int i = 0; i < 8; i++)
	{

		if (fmtCheck == fmts[i])
		{
			switch (fmts[i])
			{
			case CI8SHARED: // CI8 with shared palette
				decodeCI8image(buffer,data[i],sharedPal,32,32);
				buffer += 32*32;
				break;
			case RGB5A3: // RGB5A3
				decode5A3image(buffer, (u16*)(data[i]), 32, 32);
				break;
			case CI8: // CI8 with own palette
				u16 *paldata = (u16*)(data[i] + 32*32);
				decodeCI8image(buffer, data[i], paldata, 32, 32);
				buffer += 32*32;
				break;
			}
		}
	}

	return frames;
}


bool GCMemcard::Format(u8 * card_data, bool sjis, u16 SizeMb)
{
	if (!card_data)
		return false;
	memset(card_data, 0xFF, BLOCK_SIZE*3);
	memset(card_data + BLOCK_SIZE*3, 0, BLOCK_SIZE*2);

	GCMC_Header gcp;
	gcp.hdr = (Header*)card_data;
	gcp.dir = (Directory *)(card_data + BLOCK_SIZE);
	gcp.dir_backup = (Directory *)(card_data + BLOCK_SIZE*2);
	gcp.bat = (BlockAlloc *)(card_data + BLOCK_SIZE*3);
	gcp.bat_backup = (BlockAlloc *)(card_data + BLOCK_SIZE*4);

	*(u16*)gcp.hdr->SizeMb = BE16(SizeMb);
	*(u16*)gcp.hdr->Encoding = BE16(sjis ? 1 : 0);

	FormatInternal(gcp);
	return true;
}

bool GCMemcard::Format(bool sjis, u16 SizeMb)
{
	memset(&hdr, 0xFF, BLOCK_SIZE);
	memset(&dir, 0xFF, BLOCK_SIZE);
	memset(&dir_backup, 0xFF, BLOCK_SIZE);
	memset(&bat, 0, BLOCK_SIZE);
	memset(&bat_backup, 0, BLOCK_SIZE);

	GCMC_Header gcp;
	gcp.hdr = &hdr;
	gcp.dir = &dir;
	gcp.dir_backup = &dir_backup;
	gcp.bat = &bat;
	gcp.bat_backup = &bat_backup;
	
	*(u16*)hdr.SizeMb = BE16(SizeMb);
	*(u16*)hdr.Encoding = BE16(sjis ? 1 : 0);
	FormatInternal(gcp);

	m_sizeMb = SizeMb;
	maxBlock = (u32)m_sizeMb * MBIT_TO_BLOCKS;
	mc_data_blocks.reserve(maxBlock - MC_FST_BLOCKS);
	for (u32 i = 0; i < (maxBlock - MC_FST_BLOCKS); ++i)
	{
		GCMBlock b;
		mc_data_blocks.push_back(b);
	}

	m_valid = true;

	return Save();
}

void GCMemcard::FormatInternal(GCMC_Header &GCP)
{
	Header *p_hdr = GCP.hdr;
	u64 rand = CEXIIPL::GetGCTime();
	p_hdr->formatTime = Common::swap64(rand);

	for(int i = 0; i < 12; i++)
	{
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
		p_hdr->serial[i] = (u8)(g_SRAM.flash_id[0][i] + (u32)rand);
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);	
		rand &= (u64)0x0000000000007fffULL;
	}
	*(u32*)&p_hdr->SramBias = g_SRAM.counter_bias;
	*(u32*)&p_hdr->SramLang = g_SRAM.lang;
	// TODO: determine the purpose of Unk2 1 works for slot A, 0 works for both slot A and slot B
	*(u32*)&p_hdr->Unk2 = 0;		// = _viReg[55];  static vu16* const _viReg = (u16*)0xCC002000;
	*(u16*)&p_hdr->deviceID = 0;
	calc_checksumsBE((u16*)p_hdr, 0xFE, &p_hdr->Checksum, &p_hdr->Checksum_Inv);

	Directory *p_dir = GCP.dir,
		*p_dir_backup = GCP.dir_backup;
	*(u16*)&p_dir->UpdateCounter = 0;
	*(u16*)&p_dir_backup->UpdateCounter = BE16(1);
	calc_checksumsBE((u16*)p_dir, 0xFFE, &p_dir->Checksum, &p_dir->Checksum_Inv);
	calc_checksumsBE((u16*)p_dir_backup, 0xFFE, &p_dir_backup->Checksum, &p_dir_backup->Checksum_Inv);
	
	BlockAlloc *p_bat = GCP.bat,
		*p_bat_backup = GCP.bat_backup;
	*(u16*)&p_bat_backup->UpdateCounter = BE16(1);
	*(u16*)&p_bat->FreeBlocks = *(u16*)&p_bat_backup->FreeBlocks = BE16(( BE16(p_hdr->SizeMb) * MBIT_TO_BLOCKS) - MC_FST_BLOCKS);
	*(u16*)&p_bat->LastAllocated = *(u16*)&p_bat_backup->LastAllocated = BE16(4);
	calc_checksumsBE((u16*)p_bat+2, 0xFFE, &p_bat->Checksum, &p_bat->Checksum_Inv);
	calc_checksumsBE((u16*)p_bat_backup+2, 0xFFE, &p_bat_backup->Checksum, &p_bat_backup->Checksum_Inv);
}