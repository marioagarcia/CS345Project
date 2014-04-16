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
	int cd = CDIR;
	int sectorNumber;
	char buffer[BYTES_PER_SECTOR];

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
		fdEntry->currentCluster = dirEntry.startCluster;
		fdEntry->fileSize = dirEntry.fileSize;
		fdEntry->pid = curTask;
		fdEntry->mode = rwMode;
		fdEntry->flags = 0;
		if (rwMode == OPEN_APPEND)
			fdEntry->fileIndex = dirEntry.fileSize;
		else
			fdEntry->fileIndex = 0;

		//return the filedescriptor
		return index;

		//this belongs to a different function
		//error = fmsReadSector(buffer, C_2_S(dirEntry.startCluster)); 
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

		// fet index into slot buffer
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
			if ((error = fmsReadSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) return error;
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
	// copied read file 
	//changed numBytesRead to numBytesWritten
	int error, nextCluster;
	FDEntry* fdEntry;
	int numBytesWritten = 0;
	unsigned int  bytesLeft, bufferIndex;

	fdEntry = &OFTable[fileDescriptor];
	if (fdEntry->name[0] == 0) return ERR63;
	if (fdEntry->pid != curTask) return ERR85;
	if (fdEntry->mode == 0)
		return ERR84; // read only file

	while (nBytes > 0)
	{
			//{
			//	// get next cluster in FAT chain
			//	nextCluster = getFatEntry(fdEntry->currentCluster, FAT1);
			//	if (nextCluster == FAT_EOC) return ERR65; // no more space for files
			//}
			//// read in next cluster
			//if ((error = fmsReadSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) return error;
			//fdEntry->currentCluster = nextCluster;

		//                write								
		// limit bytes to read to minimum of what's left in buffer, what's left in file, and nBytes
		
		bytesLeft = BYTES_PER_SECTOR - fdEntry->fileIndex;
		if (bytesLeft > nBytes) bytesLeft = nBytes;
		if (bytesLeft > (BYTES_PER_SECTOR - bufferIndex))
			bytesLeft = BYTES_PER_SECTOR - bufferIndex;

		//move data from user buffer to internal buffer and update counts
		memcpy(&fdEntry->buffer[bufferIndex], buffer, bytesLeft);
		fdEntry->fileIndex += bytesLeft; 
		numBytesWritten += bytesLeft;
		//buffer -= bytesLeft;
		nBytes -= bytesLeft;
	}
	return numBytesWritten;
} // end fmsWriteFile

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	// ?? add code here
	printf("\nfmsCloseFile Not Implemented");

	return ERR63;
} // end fmsCloseFile


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
	// ?? add code here
	printf("\nfmsDefineFile Not Implemented");

	return ERR72;
} // end fmsDefineFile



// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	// ?? add code here
	printf("\nfmsDeleteFile Not Implemented");

	return ERR61;
} // end fmsDeleteFile


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
	// ?? add code here
	printf("\nfmsSeekFile Not Implemented");

	return ERR63;
} // end fmsSeekFile
