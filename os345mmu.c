// os345mmu.c - LC-3 Memory Management Unit
// **************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

extern TCB tcb[];							// task control block
extern int curTask;							// current task

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int nextPage;						// swap page size
int pageReads;						// page reads
int pageWrites;						// page writes

int getFrame(int);
int getAvailableFrame(void);
int runClock(int me);


int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >= 0) return frame;

	// run clock
	do	frame = runClock(notme);
	while (frame == -1);

	//printf("\nWe're toast!!!!!!!!!!!!");

	return frame;
}

// **************************************************************************
//Run Clock

int runClock(int me) 
{
	//printf("\nran clock alg");
	int upta, rpte1, rpte2, upte1, upte2;
	bool nonDefined = 1;

	for (int i = LC3_RPT; i < LC3_RPT_END; i += 2)
	{
		rpte1 = MEMWORD(i);
		rpte2 = MEMWORD(i + 1);

		if (DEFINED(rpte1)) 
		{
			memHits++;

			//printf("\ninside of defined rpte");
			MEMWORD(i) = CLEAR_REF(rpte1);

			for (int j = 0; j < LC3_FRAME_SIZE; j += 2) 
			{
				upta = (FRAME(rpte1) << 6) + j;
				upte1 = MEMWORD(upta);
				upte2 = MEMWORD(upta + 1);

				if (DEFINED(upte1)) 
				{
					memHits++;

					//printf("\ninside of defined upt");
					nonDefined = 0;
					if (REFERENCED(upte1))
					{
						//printf("\ninside of referenced upt");
						MEMWORD(upta) = CLEAR_REF(upte1);
					}
					else 
					{
						//swap
						//printf("\nfound a not referenced frame");
						int frame = FRAME(upte1);
						MEMWORD(upta) = CLEAR_DEFINED(upte1);
						MEMWORD(upta + 1) = swapOut(upta, upte1, upte2, frame);
						//printf("\nreturning the frame of the upt");
						return frame;
						//}
					}
				}
				else memPageFaults++;
			}
			//no data frames so swap rpte
			if (nonDefined) 
			{
				int frame = FRAME(rpte1);
				if (frame != me) 
				{
					//printf("\nwent to swap on rpte");
					MEMWORD(i) = CLEAR_DEFINED(rpte1);
					MEMWORD(i + 1) = swapOut(i, rpte1, rpte2, frame);
					//printf("\nreturning the frame of the rpte");
					return frame;
				}
			}
		}
		else memPageFaults++;
	}
	//printf("\nreturned -1");
	return -1;
}

int swapOut(int a, int e1, int e2, int frame) {
	if (PAGED(e2)) 
	{
		//if dirty
		////printf("\ndirty? %i", PAGED(e2));//printf("\nalready paged");
		//if (DIRTY(e1))
		//{
			//int dirty = DIRTY(e1);
			//printf("\ndirty? %i", DIRTY(e1));
			e2 = accessPage(SWAPPAGE(e2), frame, PAGE_OLD_WRITE);
			//MEMWORD(a) = CLEAR_DIRTY(e1);
		//}
		//else
		//{
		//	printf("\ndirty? %i", DIRTY(e1));
		//}
		////else do nothing
	}
	else 
	{
		//printf("\nfirst time paged");
		e2 = accessPage(-1, frame, PAGE_NEW_WRITE);            
	}

	return SET_PAGED(e2);
}

// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;

	rpta = tcb[curTask].RPT + RPTI(va);    //RPTI(va)          (((va)&BITS_15_11_MASK)>>10)
	rpte1 = memory[rpta];
	rpte2 = memory[rpta + 1];

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];

#if MMU_ENABLE

	if (DEFINED(rpte1))
	{
		// defined
		memHits++;
	}
	else
	{
		// fault
		memPageFaults++;
		rptFrame = getFrame(-1);
		rpte1 = SET_DEFINED(rptFrame);
		if (PAGED(rpte2))
		{
			accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
		}
		else
		{
			memset(&memory[(rptFrame << 6)], 0, 128);
			//memory[rpta] = SET_DIRTY(memory[rpta]);
		}
	}

	memory[rpta] = rpte1 = SET_REF(rpte1);
	memory[rpta + 1] = rpte2;

	upta = (FRAME(rpte1) << 6) + UPTI(va);
	upte1 = memory[upta];
	upte2 = memory[upta + 1];

	if (DEFINED(upte1))
	{
		// defined
		memHits++;
	}
	else
	{
		// fault
		memPageFaults++;
		uptFrame = getFrame(FRAME(memory[rpta]));
		upte1 = SET_DEFINED(uptFrame);
		if (PAGED(upte2))
		{
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		}
		else
		{
			//memory[upta] = SET_DIRTY(memory[upta]);
		}
	}

	if (rwFlg != 0) 
	{
		//memory[rpta] = SET_DIRTY(memory[rpta]);
		//memory[upta] = SET_DIRTY(memory[upta]);
	}

	memory[upta] = SET_REF(upte1);
	memory[upta + 1] = upte2;

	return &memory[(FRAME(upte1) << 6) + FRAMEOFFSET(va)];
#else
	pa = va;

#endif
	return &memory[va];
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{
	int i, data;
	int adr = LC3_FBT - 1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i = 0; i < LC3_FRAMES; i++)
	{
		if (fmask & 0x0001)
		{
			fmask = 0x8000;
			adr++;
			data = (flg) ? MEMWORD(adr) : 0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ((i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i = 0; i < LC3_FRAMES; i++)		// look thru all frames
	{
		if (fmask & 0x0001)
		{
			fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{
			MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
	{
		printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
		exit(-4);
	}
	switch (rwnFlg)
	{
	case PAGE_INIT:                    		// init paging
		nextPage = 0;
		return 0;

	case PAGE_GET_ADR:                    	// return page address
		return (int)(&swapMemory[pnum << 6]);

	case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
		pnum = nextPage++;

	case PAGE_OLD_WRITE:                   // write
		//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
		memcpy(&swapMemory[pnum << 6], &memory[frame << 6], 1 << 7);
		pageWrites++;
		return pnum;

	case PAGE_READ:                       // read
		//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
		memcpy(&memory[frame << 6], &swapMemory[pnum << 6], 1 << 7);
		pageReads++;
		return pnum;

	case PAGE_FREE:                   // free page
		break;
	}
	return pnum;
} // end accessPage



