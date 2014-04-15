// os345p3.c - Jurassic Park
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"
#include "DeltaClk.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
extern TCB tcb[];
extern int curTask;
extern Semaphore* taskSems[MAX_TASKS];
extern DeltaClk* dcq;

Semaphore* parkAccess;
Semaphore* tickets;
Semaphore* giftShop;
Semaphore* museum;
Semaphore* driver;
Semaphore* needDriver;
Semaphore* needTicket;
Semaphore* driverDoneSemaphore;
Semaphore* driverReady;
Semaphore* carReady;
Semaphore* needTicket;
Semaphore* takeTicket;
Semaphore* driverMutex;
Semaphore* seatAvailable;
Semaphore* readyForSeat;
Semaphore* arrived;
Semaphore* seated;

int carID;
int currCarID;
int driverID = 0;
// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
int dcMonitorTask(int argc, char* argv[]);
int timeTask(int argc, char* argv[]);
int carTask(int argc, char* argv[]);
int visitorTask(int argc, char* argv[]);
int driverTask(int argc, char* argv[]);

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char carName[32];
	char* newArgv[2];
	char* newArgv2[2];
	int i;
	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask(buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	parkAccess = createSemaphore("Park Access", COUNTING, MAX_IN_PARK);			SWAP;
	tickets = createSemaphore("Tickets", COUNTING, MAX_TICKETS);				SWAP;
	museum = createSemaphore("Museum", COUNTING, MAX_IN_MUSEUM);				SWAP;
	giftShop = createSemaphore("Gift Shop", COUNTING, MAX_IN_GIFTSHOP);			SWAP;
	driver = createSemaphore("Driver", BINARY, 0);								SWAP;
	needDriver = createSemaphore("Need Driver", BINARY, 0);						SWAP;
	driverReady = createSemaphore("Driver Ready", BINARY, 0);					SWAP;
	needTicket = createSemaphore("Need Ticket", BINARY, 0);						SWAP;
	carReady = createSemaphore("Car Ready", BINARY, 0);							SWAP;
	takeTicket = createSemaphore("Take Ticket", BINARY, 0);						SWAP;
	driverMutex = createSemaphore("Driver Mutex", BINARY, 1);					SWAP;
	readyForSeat = createSemaphore("Ready For Seat", BINARY, 0);				SWAP;
	seatAvailable = createSemaphore("Seat Available", BINARY, 0);				SWAP;
	seated = createSemaphore("Seated", BINARY, 0);								SWAP;
	
	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	carID = 0;
	for (i = 1; i <= 4; i++)
	{
		sprintf(carName, "Car %d", i);											SWAP;
		createTask(carName,	carTask, MED_PRIORITY, 0, 0);						SWAP;
		carID++;
	}

	for (i = 1; i <= NUM_VISITORS; i++)
	{
		char name[32];															SWAP;
		sprintf(name, "Visitor %d", i);											SWAP;
		createTask(name, visitorTask, MED_PRIORITY, 0, 0);						SWAP;
	}

	for (i = 1; i <= NUM_DRIVERS; i++)
	{
		char name[32];
		sprintf(name, "Driver %d", i);											SWAP;
		createTask(name, driverTask, MED_PRIORITY, 0, 0); 						SWAP;
		driverID++;
	}

	return 0;
} // end project3

int carTask(int argc, char* argv[])
{
	int i;
	int temp = carID;
	Semaphore* sem[4];
	while (1)
	{
		for (i = 0; i < 3; i++)
		{ 
			SEM_WAIT(fillSeat[temp]);					SWAP;
			SEM_SIGNAL(seatAvailable);					SWAP;
			SEM_WAIT(readyForSeat);						SWAP;
			sem[i] = arrived;
			SEM_SIGNAL(seated);							SWAP;
			
			if (i == 2)
			{
				SEM_WAIT(driverMutex);					SWAP;
				SEM_SIGNAL(needDriver);					SWAP;
				currCarID = temp;
				SEM_SIGNAL(driver);						SWAP;
				SEM_WAIT(driverReady);					SWAP;
				sem[3] = driverDoneSemaphore;			SWAP;
				SEM_SIGNAL(driverMutex);				SWAP;
			}
			
			SEM_SIGNAL(seatFilled[temp]);				SWAP;
		}
		
		SEM_WAIT(rideOver[temp]);						SWAP;
		for (i = 0; i < 4; i++)
			SEM_SIGNAL(sem[i]);							SWAP;
	}
	return 0;
}

int driverTask(int argc, char* argv[])
{
	char name[32];
	int id = driverID;
	sprintf(name, "Driver Done %d", id);
	Semaphore* driverDone = createSemaphore(name, BINARY, 0);

	while (1)
	{
		
		SEM_WAIT(parkMutex);							SWAP;
		myPark.drivers[id] = 0;							SWAP;
		SEM_SIGNAL(parkMutex);							SWAP;
		SEM_WAIT(driver);								SWAP;

		if (semTryLock(needDriver))
		{
			driverDoneSemaphore = driverDone;
			
			SEM_WAIT(parkMutex);						SWAP;
			myPark.drivers[id] = currCarID + 1;			SWAP;
			SEM_SIGNAL(parkMutex);						SWAP;
			
			SEM_SIGNAL(driverReady);					SWAP;
			SEM_WAIT(driverDone);						SWAP;
		}
		else if (semTryLock(needTicket))
		{
			SEM_WAIT(parkMutex);						SWAP;
			myPark.drivers[id] = -1;					SWAP;
			SEM_SIGNAL(parkMutex);						SWAP;
			SEM_SIGNAL(takeTicket);						SWAP;
		}
		else break;
	}
	return 0;
}

int visitorTask(int argc, char* argv[])
{
	int randomTime, id;									SWAP;
	id = curTask;										SWAP;
	srand(time(NULL));									SWAP;
	
	//Randomly arrive at park
	randomTime = (rand() % 10) * 10;					SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numOutsidePark++;							SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Wait randomly for entrance to the park
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	SEM_WAIT(parkAccess);								SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numOutsidePark--;							SWAP;
	myPark.numInPark++;									SWAP;
	myPark.numInTicketLine++;							SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Wait randomly in line for a ticket
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	SEM_WAIT(tickets);									SWAP;
	
	//Wake up the driver and ask for a ticket, then wait for the ticket
	SEM_WAIT(driverMutex);								SWAP;
	SEM_SIGNAL(needTicket);								SWAP;
	SEM_SIGNAL(driver);									SWAP;
	SEM_WAIT(takeTicket);								SWAP;
	SEM_SIGNAL(driverMutex);							SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInTicketLine--;							SWAP;
	myPark.numTicketsAvailable--;						SWAP;
	myPark.numInMuseumLine++;							SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Wait randomly for entrance to the museum
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	SEM_WAIT(museum);									SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInMuseumLine--;							SWAP;
	myPark.numInMuseum++;								SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Wait randomly to exit museum
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInMuseum--;								SWAP;
	myPark.numInCarLine++;								SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Release spot in the museum and wait for a seat
	SEM_SIGNAL(museum);									SWAP;
	SEM_WAIT(seatAvailable);							SWAP;
	arrived = taskSems[id];								SWAP;
	
	//Signal that you're ready for a seat, then wait to be seated
	SEM_SIGNAL(readyForSeat);							SWAP;
	SEM_WAIT(seated);									SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInCarLine--;								SWAP;
	myPark.numTicketsAvailable++;						SWAP;
	myPark.numInCars++;									SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Release your ticket and wait to be released by the car
	SEM_SIGNAL(tickets);								SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInCars--;									SWAP;
	myPark.numInGiftLine++;								SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Wait randomly to enter the gift shop
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	SEM_WAIT(giftShop);									SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInGiftShop++;								SWAP;
	myPark.numInGiftLine--;								SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Randomly wait to exit giftshop
	randomTime = rand() % 30;							SWAP;
	insertDeltaClock(dcq, randomTime, taskSems[id]);	SWAP;
	SEM_WAIT(taskSems[id]);								SWAP;
	
	//Update park stats
	SEM_WAIT(parkMutex);								SWAP;
	myPark.numInGiftShop--;								SWAP;
	myPark.numInPark--;									SWAP;
	myPark.numExitedPark++;								SWAP;
	SEM_SIGNAL(parkMutex);								SWAP;
	
	//Release your spot in the museum and in the park
	SEM_SIGNAL(giftShop);								SWAP;
	SEM_SIGNAL(parkAccess);								SWAP;

	return 0;
}






// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
int i;
printf("\nDelta Clock");
// ?? Implement a routine to display the current delta clock contents
//printf("\nTo Be Implemented!");
for(i = 0; i < dcq->count - 1; i++)
{
//printf("\n%4d%4d  %20s", i, dcq->deltaClockEvents[i].time, dcq->deltaClockEvents[i].sem.name);
}
return 0;
} // end CL3_dc