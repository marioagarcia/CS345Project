#include<stdlib.h>
#include<stdio.h>
#include "DeltaClk.h"

extern Semaphore* countMutex;

void insertDeltaClock(DeltaClk *dclk, int time, Semaphore *sem) {

	Event *e;														SWAP;
	e = (Event*)malloc(sizeof(Event));								SWAP;
	e->sem = sem;													SWAP;
	e->time = time;													SWAP;

	SEM_WAIT(countMutex);											SWAP;
	if (dclk->count == 0) {
		dclk->list[0] = e;											SWAP;
		dclk->count++;												SWAP;
	}
	else {

		int i = 0;
		while (1)
		{

			if (e->time > dclk->list[i]->time) {
				e->time -= dclk->list[i]->time;						SWAP;
				if (i == dclk->count - 1) {
					dclk->list[++i] = e;							SWAP;
					dclk->count++;									SWAP;
					break;
				}
				else i++;											SWAP;
			}

			else {
				for (int j = dclk->count - 1; j >= i; j--)	{
					dclk->list[j + 1] = dclk->list[j];				SWAP;
				}
				dclk->list[i] = e;									SWAP;
				dclk->count++;										SWAP;
				dclk->list[i + 1]->time -= e->time;					SWAP;
				break;
			}
		}
	}
	SEM_SIGNAL(countMutex);											SWAP;
}

void decrementDeltaClock(DeltaClk *dclk) {
	if (dclk->count > 0) {

		if (dclk->list[0]->time == 0) {
			while (dclk->count > 0) {
				if (dclk->list[0]->time == 0) {
					semSignal(dclk->list[0]->sem);
					for (int j = 0; j < dclk->count; j++) {
						dclk->list[j] = dclk->list[j + 1];
					}
					dclk->count--;
				}
				else {
					break;
				}
			}
		}
		else {
			dclk->list[0]->time--;
		}

	}
}

DeltaClk* initDeltaClk() {
	DeltaClk * dclk = (DeltaClk*)malloc(sizeof(DeltaClk));
	dclk->count = 0;
	for (size_t i = 0; i < MAX_ARGS; i++)
	{
		dclk->list[i] = 0;
	}
	return dclk;
}