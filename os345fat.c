// os345fat.c - file management system
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);

void copyUint8(uint8 * dest, uint8 * src);
int compUint8(uint8 * first, uint8 * second);
void toCstr(uint8 * name, uint8 * ext, char * dest);
int findSpaceForSector(DirEntry * buffer);
int compareFileNames(DirEntry * buffer, char * fileName);
int checkDirEmpty(DirEntry dirEntry);
int deleteFromSector(DirEntry * buffer, char * fileName);

int fmsSetDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

int getFreeFatEntry();
extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FDEntry OFTable[NFILES];					// open file table

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #

// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//
int fmsOpenFile(char* fileName, int rwMode)
{
	DirEntry dirEntry;
	int sectorNumber;

	int error = fmsGetDirEntry(fileName, &dirEntry);
	if (!error)
	{
		//check if file is already open
		for (int i = 0; i < NFILES; i++)
		{
			//if open then return error
			if (compUint8(OFTable[i].name, dirEntry.name)) {
				return ERR62;
			}
		}

		//Fill out fdEntry and put in OFTable and return the index

		FDEntry * fdEntry = NULL;
		int index;

		//Find an open spot in the OFTable
		for (int i = 0; i < NFILES; i++)
		{
			if (OFTable[i].name[0] == 0 || OFTable[i].name[0] == 0xe5)
			{
				fdEntry = &OFTable[i];
				index = i;
				break;
			}
		}
		if (fdEntry == NULL) return ERR70; // Too many files opened

		// fill in the fdEntry with the info in dirEntry
		copyUint8(fdEntry->name, dirEntry.name);				//this is a helper function I made
		copyUint8(fdEntry->extension, dirEntry.extension);
		fdEntry->attributes = 0;
		fdEntry->directoryCluster = CDIR;
		fdEntry->startCluster = dirEntry.startCluster;
		fdEntry->currentCluster = 0;
		fdEntry->fileSize = dirEntry.fileSize;
		fdEntry->pid = curTask;
		fdEntry->mode = rwMode;
		fdEntry->flags = 0;
		if (rwMode & OPEN_APPEND)
			fdEntry->fileIndex = dirEntry.fileSize;
		else
			fdEntry->fileIndex = 0;

		//return the filedescriptor
		return index;
	}

} // end fmsOpenFile


// ***********************************************************************
// Helper functions

void copyUint8(uint8 * dest, uint8 * src)
{
	for (int i = 0; i < 8; i++)
	{
		dest[i] = src[i];
	}
}

int compUint8(uint8 * first, uint8 * second)
{
	for (int i = 0; i < 8; i++)
	{
		if (first[i] != second[i])
		{
			return 0;
		}
	}

	return 1;
}

// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int fmsReadFile(int fileDescriptor, char* buffer, int nBytes)
// Called by copy command.
// This function reads nBytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (watch for the end of file), or -1 for
// failure. (If you are already at the end of file, return 0 bytes successfully read)
//
//		ERR63 = file not open
//		ERR66 = end of file
//		ERR85 = illegal access
//
//					|----100----|----101----|----102----|----
//		fileIndex		0			512			1024		1536
//		startCluster	100			
//		currentCluster	100			100			101			102
//
{
	int error, nextCluster;
	FDEntry* fdEntry;
	int numBytesRead = 0;
	unsigned int  bytesLeft, bufferIndex;

	fdEntry = &OFTable[fileDescriptor];
	if (fdEntry->name[0] == 0) return ERR63;
	if (fdEntry->pid != curTask) return ERR85;
	if ((fdEntry->mode == 1) || (fdEntry->mode == 2))
		return ERR85; // illegal access (write or append only)

	while (nBytes > 0)
	{
		// check for EOF
		if (fdEntry->fileSize == fdEntry->fileIndex) return (numBytesRead ? numBytesRead : ERR66);

		// get index into slot buffer
		bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;

		// check for buffer boundary
		if ((bufferIndex == 0) && (fdEntry->fileIndex || !fdEntry->currentCluster))
		{
			// check initial cluster
			if (fdEntry->currentCluster == 0)
			{
				//read intitial cluster
				if (fdEntry->startCluster == 0) return ERR66;  //eof
				nextCluster = fdEntry->startCluster;
				fdEntry->fileIndex = 0;
			}
			else
			{
				// get next cluster in FAT chain
				nextCluster = getFatEntry(fdEntry->currentCluster, FAT1);
				if (nextCluster == FAT_EOC) return numBytesRead; // end of read
			}
			if (fdEntry->flags & BUFFER_ALTERED)
			{
				//write out current Cluster if altered
				if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) return error;
				fdEntry->flags &= ~BUFFER_ALTERED;
			}
			// read in next cluster
			if ((error = fmsReadSector(fdEntry->buffer, C_2_S(nextCluster)))) return error;
			fdEntry->currentCluster = nextCluster;
		}

		// limit bytes to read to minimum of what's left in buffer, what's left in file, and nBytes
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytes) bytesLeft = nBytes;
		if (bytesLeft > (fdEntry->fileSize - fdEntry->fileIndex))
			bytesLeft = fdEntry->fileSize - fdEntry->fileIndex;

		// move data from internal buffer to user buffer and update counts
		memcpy(buffer, &fdEntry->buffer[bufferIndex], bytesLeft);
		fdEntry->fileIndex += bytesLeft;
		numBytesRead += bytesLeft;
		buffer += bytesLeft;
		nBytes -= bytesLeft;
	}
	return numBytesRead;
} // end fmsReadFile

// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytes)
{
	int error, nextCluster;
	char fileName[13];
	DirEntry dirEntry;
	FDEntry* fdEntry;
	int numBytesWritten = 0;
	unsigned int bytesLeft, bufferIndex;

	fdEntry = &OFTable[fileDescriptor];
	if (fdEntry->name[0] == 0) return ERR63;
	if (fdEntry->pid != curTask) return ERR85;
	if (fdEntry->mode == 0) return ERR84;

	//Get a new FAT cluster if a new file
	if (fdEntry->startCluster == 0)
	{
		int start = getFreeFatEntry();
		setFatEntry(start, FAT_EOC, FAT1);
		fdEntry->startCluster = start;
	}

	while (nBytes > 0)
	{
		int numClusters, fatEntry, i;
		FDEntry* tempEntry = &OFTable[fileDescriptor];

		bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;

		tempEntry->currentCluster = tempEntry->startCluster;
		numClusters = tempEntry->fileIndex / BYTES_PER_SECTOR;

		for (i = 0; i < numClusters; i++)
		{
			nextCluster = getFatEntry(tempEntry->currentCluster, FAT1);

			if (nextCluster == FAT_EOC)
			{
				fatEntry = getFreeFatEntry();
				setFatEntry(fatEntry, FAT_EOC, FAT1);
				setFatEntry(tempEntry->currentCluster, fatEntry, FAT1);
				tempEntry->currentCluster = fatEntry;
			}
			else
			{
				tempEntry->currentCluster = nextCluster;
			}
		}

		if ((error = fmsReadSector(tempEntry->buffer, C_2_S(tempEntry->currentCluster)))) return error;

		//Get number of bytes we can write 
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytes)
			bytesLeft = nBytes;

		memcpy(&fdEntry->buffer[bufferIndex], buffer, bytesLeft);

		buffer += bytesLeft;
		nBytes -= bytesLeft;
		numBytesWritten += bytesLeft;
		fdEntry->fileIndex += bytesLeft;

		if (fdEntry->fileIndex > fdEntry->fileSize)
			fdEntry->fileSize = fdEntry->fileIndex;

		fdEntry->flags |= BUFFER_ALTERED;

		if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) return error;
	}

	//populate the info back to the file??
	toCstr(fdEntry->name, fdEntry->extension, fileName);

	if (error = fmsGetDirEntry(fileName, &dirEntry)) return error;

	dirEntry.startCluster = fdEntry->startCluster;
	dirEntry.fileSize = fdEntry->fileSize;
	setDirTimeDate(&dirEntry);

	int dirNum = 0;

	if (error = fmsSetDirEntry(&dirNum, fileName, &dirEntry, CDIR)) return error;

	return numBytesWritten;
} // end fmsWriteFile

// ***********************************************************************
//Write file helper functions

int fmsSetDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir)
{
	DirEntry tempDirEntry;
	char buffer[BYTES_PER_SECTOR];
	int dirIndex, dirSector, error;
	int loop = *dirNum / ENTRIES_PER_SECTOR;
	int dirCluster = dir;

	while (1)
	{
		// load directory sector
		if (dir)
		{	// sub directory
			while (loop--)
			{
				dirCluster = getFatEntry(dirCluster, FAT1);
				if (dirCluster == FAT_EOC) return ERR67;
				if (dirCluster == FAT_BAD) return ERR54;
				if (dirCluster < 2) return ERR54;
			}
			dirSector = C_2_S(dirCluster);
		}
		else
		{	// root directory
			dirSector = (*dirNum / ENTRIES_PER_SECTOR) + BEG_ROOT_SECTOR;
			if (dirSector >= BEG_DATA_SECTOR) return ERR67;
		}

		// read sector into directory buffer
		if (error = fmsReadSector(buffer, dirSector)) return error;

		// find next matching directory entry
		while (1)
		{	// read directory entry
			dirIndex = *dirNum % ENTRIES_PER_SECTOR;
			memcpy(&tempDirEntry, &buffer[dirIndex * sizeof(DirEntry)], sizeof(DirEntry));
			if (tempDirEntry.name[0] == 0) return ERR67;	// EOD
			(*dirNum)++;                        		// prepare for next read
			if (tempDirEntry.name[0] == 0xe5);     		// Deleted entry, go on...
			else if (tempDirEntry.attributes == LONGNAME);
			else if (fmsMask(mask, tempDirEntry.name, tempDirEntry.extension))
			{
				//populate buffer with the dirEntry passed in
				memcpy(&buffer[dirIndex * sizeof(DirEntry)], dirEntry, sizeof(DirEntry));

				// write buffer into directory sector 
				if (error = fmsWriteSector(buffer, dirSector)) return error;

				return 0;
			}
			// break if sector boundary
			if ((*dirNum % ENTRIES_PER_SECTOR) == 0) break;
		}
		// next directory sector/cluster
		loop = 1;
	}
	return 0;
}

int getFreeFatEntry()
{
	int i;
	//int numEntries = (BYTES_PER_SECTOR * NUM_FAT_SECTORS) / 1.5;
	for (i = BEG_DATA_SECTOR; i < CLUSTERS_PER_DISK; i++)
	{
		if (getFatEntry(i, FAT1) == 0)
			return i;
	}
	return ERR65;
}

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	int error, i, j;
	char fileName[12];
	DirEntry dirEntry;
	FDEntry* fdEntry = &OFTable[fileDescriptor];

	if (fdEntry->name[0] == 0) return ERR63;  //File not open
	if (fdEntry->pid != curTask) return ERR85;  //Illegal access

	if (fdEntry->flags & BUFFER_ALTERED)  //Write out current cluster if altered
	{
		if (error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster))) return error;
	}

	fdEntry->name[0] = 0;

	return 0;
} // end fmsCloseFile

// ***********************************************************************
void toCstr(uint8 * name, uint8 * ext, char * dest)
{
	size_t i;

	for (i = 0; i < 8; i++)	{
		if (name[i] != 32)
			dest[i] = name[i];
		else break;
	}

	dest[i] = '.';

	for (size_t j = 0; j < 3; j++) {
		if (ext[j] != 32)
			dest[++i] = ext[j];
		else break;
	}

	dest[++i] = 0;
}

// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute)
{
	if (!diskMounted) return ERR72;

	int freeIndex = -1;
	int freeSector = -1;
	DirEntry buffer[ENTRIES_PER_SECTOR];

	if (CDIR == 0)
	{
		for (size_t i = BEG_ROOT_SECTOR; i < BEG_DATA_SECTOR; i++)
		{
			fmsReadSector(&buffer, i);
			if (freeIndex == -1)
			{
				freeIndex = findSpaceForSector(&buffer);
				freeSector = i;
			}
			if (compareFileNames(&buffer, fileName)) return ERR60;
		}
		if (freeIndex == -1)
			return ERR65;
	}
	else
	{
		int currentCluster = CDIR;
		int prevCluster;
		while (currentCluster != FAT_EOC)
		{
			prevCluster = currentCluster;

			fmsReadSector(&buffer, C_2_S(currentCluster));

			if (freeIndex == -1)
			{
				freeIndex = findSpaceForSector(&buffer);
				freeSector = C_2_S(currentCluster);
			}

			if (compareFileNames(&buffer, fileName)) return ERR60;

			currentCluster = getFatEntry(currentCluster, FAT1);
		}

		if (freeIndex == -1)
		{
			freeIndex = 0;
			int freeFatEntry = getFreeFatEntry();
			setFatEntry(prevCluster, freeFatEntry, FAT1);
			setFatEntry(freeFatEntry, FAT_EOC, FAT1);
			freeSector = C_2_S(freeFatEntry);

			memset(buffer, 0, sizeof(buffer));
			fmsWriteSector(&buffer, freeSector);
		}
	}

	fmsReadSector(&buffer, freeSector);
	int i = 0;
	for (i = 0; i < 8; i++)
	{
		buffer[freeIndex].name[i] = ' ';
	}
	for ( i = 0; i < 8; i++)
	{
		if (fileName[i] == 0)
			break;
		if (fileName[i] == '.')
			break;
		buffer[freeIndex].name[i] = fileName[i];
	}
	for (int j = 0; j < 3; j++)
	{
		buffer[freeIndex].extension[j] = ' ';
	}
	if (fileName[i++] == '.')
	{
		for (int k = 0; k < 3; i++, k++)
		{
			if (fileName[i] == 0) break;
			buffer[freeIndex].extension[k] = fileName[i];
		}
	}

	if (attribute & DIRECTORY)
	{
		int dirCluster = getFreeFatEntry();
		if (dirCluster < 0) return ERR65;
		setFatEntry(dirCluster, FAT_EOC, FAT1);
		DirEntry dirBuffer[ENTRIES_PER_SECTOR];
		memset(dirBuffer, 0, sizeof(dirBuffer));
		memset(dirBuffer[0].name, 0x20, sizeof(dirBuffer[0].name));
		memset(dirBuffer[1].name, 0x20, sizeof(dirBuffer[1].name));
		memset(dirBuffer[0].extension, 0x20, sizeof(dirBuffer[0].extension));
		memset(dirBuffer[1].extension, 0x20, sizeof(dirBuffer[1].extension));

		// .
		dirBuffer[0].name[0] = '.';
		dirBuffer[0].attributes = DIRECTORY;
		dirBuffer[0].startCluster = dirCluster;
		setDirTimeDate(&dirBuffer[0]);


		// ..
		dirBuffer[1].name[0] = '.';
		dirBuffer[1].name[1] = '.';
		dirBuffer[1].attributes = DIRECTORY;
		dirBuffer[1].startCluster = CDIR;
		setDirTimeDate(&dirBuffer[1]);

		fmsWriteSector(&dirBuffer, C_2_S(dirCluster));

		buffer[freeIndex].startCluster = dirCluster;
	}
	else
	{
		buffer[freeIndex].startCluster = 0;
	}
	buffer[freeIndex].attributes = attribute;
	buffer[freeIndex].fileSize = 0;
	setDirTimeDate(&buffer[freeIndex]);
	fmsWriteSector(&buffer, freeSector);
	return 0;
} // end fmsDefineFile

int findSpaceForSector(DirEntry * buffer)
{
	for (size_t i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		if (buffer[i].name[0] == 0 || buffer[i].name[0] == 0xe5)
		{
			return i;
		}
	}
	return -1;
}

int compareFileNames(DirEntry * buffer, char * fileName)
{
	for (size_t i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		if (fmsMask(fileName, buffer[i].name, buffer[i].extension))
		{
			return ERR65;
		}
	}
	return 0;
}

// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	DirEntry buffer[ENTRIES_PER_SECTOR];
	
	if (!diskMounted) return ERR72;

	if (CDIR == 0)
	{
		for (size_t i = BEG_ROOT_SECTOR; i < BEG_DATA_SECTOR; i++)
		{
			fmsReadSector(&buffer, i);
			int error = deleteFromSector(&buffer, fileName);
			switch (error)
			{
			case 0:
				fmsWriteSector(&buffer, i);
				return 0;
				break;
			case -1:
				continue;
				break;
			default:
				return error;
			}
		}
	}
	else
	{
		int currentCluster = CDIR;
		while (currentCluster != FAT_EOC)
		{
			fmsReadSector(&buffer, C_2_S(currentCluster));
			int error = deleteFromSector(&buffer, fileName);
			switch (error)
			{
			case 0:
				fmsWriteSector(&buffer, C_2_S(currentCluster));
				return 0;
				break;
			case -1:
				currentCluster = getFatEntry(currentCluster, FAT1);
				break;
			default:
				return error;
			}
		}
	}
	return ERR61;
} // end fmsDeleteFile

int deleteFromSector(DirEntry * buffer, char * fileName)
{
	for (size_t i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		if (buffer[i].name[0] == 0) return -1;
		if (fmsMask(fileName, buffer[i].name, buffer[i].extension))
		{
			if (!checkDirEmpty(buffer[i])) return ERR69;
			buffer[i].name[0] = 0xe5;
			int currentCluster = buffer[i].startCluster;
			if (currentCluster == 0) return 0; //deleted a brand new file
			while (currentCluster != FAT_EOC)
			{
				int tempCluster = getFatEntry(currentCluster, FAT1);
				setFatEntry(currentCluster, 0, FAT1);
				currentCluster = tempCluster;
			}
			return 0;
		}
	}
	return -1;
}

int checkDirEmpty(DirEntry dirEntry)
{
	DirEntry tempDirEntry;
	int index = 0;
	char mask[20];
	strcpy(mask, "*.*");

	if (!(dirEntry.attributes & DIRECTORY)) return 1;
	else
	{
		while (fmsGetNextDirEntry(&index, mask, &tempDirEntry, dirEntry.startCluster) != ERR67)
		{
			if (!fmsMask(".", tempDirEntry.name, tempDirEntry.extension) 
				&& !fmsMask("..", tempDirEntry.name, tempDirEntry.extension))
				return 0;
		}
	}
	return 1;
}

// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index)
{
	FDEntry* currentFile = &OFTable[fileDescriptor];

	if (!diskMounted) return ERR72;

	if (fileDescriptor < 0 || fileDescriptor > NFILES) return ERR52;


	if (currentFile->name[0] == 0) return ERR63;

	if (index > currentFile->fileSize || index < 0) return ERR80;

	if (currentFile->flags & BUFFER_ALTERED)
	{
		int error;

		if ((error = fmsWriteSector(currentFile->buffer, C_2_S(currentFile->currentCluster)))) return error;

		currentFile->flags &= ~BUFFER_ALTERED;
	}

	currentFile->fileIndex = index;

	int end = index / (BYTES_PER_SECTOR + 1);
	currentFile->currentCluster = currentFile->startCluster;

	for (size_t i = 0; i < end; i++)
	{
		currentFile->currentCluster = getFatEntry(currentFile->currentCluster, FAT1);
	}

	if (currentFile->currentCluster != FAT_EOC)
		fmsReadSector(currentFile->buffer, C_2_S(currentFile->currentCluster));

	return currentFile->fileIndex;

} // end fmsSeekFile
