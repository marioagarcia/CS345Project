#ifndef DELTACLK_H
#define DELTACLK_H

#include <setjmp.h>
#include "os345.h"

typedef struct 
{
	int time;
	Semaphore *sem;
} Event;

typedef struct 
{
	Event* list[MAX_TASKS];
	int count;
	//bool mutexLock;
} DeltaClk;

void insertDeltaClock(DeltaClk *dclk, int time, Semaphore *sem);
void decrementDeltaClock(DeltaClk *dclk);

DeltaClk * initDeltaClk();


#endif

