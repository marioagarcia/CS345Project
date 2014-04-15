#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "Queue.h"

task enQ(PQueue* pq, int tid, int p) {

	task newTask;
	newTask.priority = p;
	newTask.tid = tid;
	
	pq->queue[pq->count++] = newTask;

	for (int i = pq->count - 1; i >= 0; i--) {
		if (newTask.priority <= pq->queue[i].priority) {
			task temp = pq->queue[i];
			pq->queue[i] = newTask;
			pq->queue[i + 1] = temp;
		}
	}
	return newTask;
}

task deQ(PQueue* pq, int tid) {
	//if(tid >= 0) {
	for (int i = pq->count - 1; i >= 0; i--) {
		if (pq->queue[i].tid == tid) {
			task newTask = pq->queue[i];
			for (int j = i; j < pq->count - 1; j++) {
				pq->queue[j] = pq->queue[j + 1];
			}
			pq->count--;
			return newTask;
		}
	}
	task notFound;
	notFound.tid = -1;
	return notFound;
	//}
}

task deQtop(PQueue* pq) {
	if (pq->count > 0) {
		return  pq->queue[--pq->count];
	}
	else {
		task notFound;
		notFound.tid = -1;
		return notFound;
	}
}

PQueue* initQueue() {
	PQueue * queue = (PQueue*)malloc(sizeof(PQueue));
	queue->count = 0;
	return queue;
}

void toStringQ(PQueue queue) {
	for (int i = 0; i < queue.count; i++) {

	}
}


